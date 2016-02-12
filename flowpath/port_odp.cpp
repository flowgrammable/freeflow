#include "port_odp.hpp"
#include "packet.hpp"
#include "context.hpp"

//#include <arpa/inet.h>
//#include <netinet/in.h>
#include <stdio.h>
#include <cstring>
#include <string>
#include <algorithm>

#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <iostream>

#include <exception>


// The ODP port module.

namespace fp
{


// ODP Port constructor. Parses the device name from
// the input string given, allocates a new internal ID.
Port_odp::Port_odp(Port::Id id, std::string const& args)
  : Port(id, "")
{
  std::string name = args;

  // Sanity check length of device name.
  if (name.length() < 1)
    throw std::string("bad device name");

  // Set the port name.
  name_ = name;

  // Initialize ODP pktio device.
  odp_pktio_param_t pktio_param;
  odp_pktio_param_init(&pktio_param);

  // Define pktio mode.
  // - ODP_PKTIN_MODE_RECV: burst recv() mode
  // - ODP_PKTIN_MODE_POLL: polled event queue
  // - ODP_PKTIN_MODE_SCHED: scheduled event queue
  pktio_param.in_mode = ODP_PKTIN_MODE_RECV;

  // Determine packet pool.
  // TODO: pktpool created in main and looked up by constant name;
  //       have port initialize pools as needed!
  pktpool_ = odp_pool_lookup(PKT_POOL_NAME);
  if (pktpool_ == ODP_POOL_INVALID)
    throw std::string("Error: failed to lookup pktpool by name: ").append(PKT_POOL_NAME);

  // Create pktio instance.
  pktio_ = odp_pktio_open(name_.c_str(), pktpool_, &pktio_param);
  if (pktio_ == ODP_PKTIO_INVALID)
    throw std::string("Error: pktio create failed for: " + name_);
}


// UDP Port destructor. Releases the allocated ID.
Port_odp::~Port_odp()
{ }


// Start recieving and transmitting.
int
Port_odp::open()
{
  int ret = odp_pktio_start(pktio_);
  // TODO: how should port open error be reported?
  if (ret != 0)
    throw std::string("Error: unable to start pktio dev");
  return ret;
}


// Stop recieving and transmitting.
int
Port_odp::close()
{
  int ret = odp_pktio_stop(pktio_);
  // TODO: how should port close error be reported?
  if (ret != 0)
    throw std::string("Error: unable to stop pktio dev");
  return ret;
}


// Read packets from the socket.
// Returns number of sucessfully received packets.
int
Port_odp::recv()
{
  // Check if port is usable.
  if (config_.down)
    throw std::string("unable to recv, port down");

  // Recieve batch of packets.
  odp_packet_t pkt_tbl[MAX_PKT_BURST];
  int pkts = odp_pktio_recv(pktio_, pkt_tbl, MAX_PKT_BURST);
  if (pkts < 0)
    throw std::string("Error: unable to recv on pktio dev");

  // Wrap packets in packet context.
  odp_time_t arrival = odp_time_global();
  uint64_t bytes = 0;
  for (int i = 0; i < pkts; i++) {
    odp_packet_t odp_pkt = pkt_tbl[i];

    // Create a new packet, leaving the data in the original buffer.
    // TODO: note that ODP can have multiple segments / packet.
    //   This is exactly what we want.  Need to fully support this!
    if (odp_packet_is_segmented(odp_pkt)) {
      printf("Unhandled: packet is segmented...\n");
    }
    void* seg_buf = odp_packet_data(odp_pkt);
    uint32_t seg_len = odp_packet_len(odp_pkt);
    uint64_t arrival_ns = odp_time_to_ns(arrival);
    Packet* pkt = packet_create((unsigned char*)seg_buf, seg_len, arrival_ns, odp_pkt, fp::FP_BUF_ODP);

    // Create a new context associated with the packet.
    Context* cxt = new Context(pkt, id_, id_, 0, 0, 0);

    // Sum up stats for all recieved packets.
    bytes += seg_len;

    // Send context to pipeline.
    thread_pool.app()->lib().pipeline(cxt);
  }

  // Update port stats.
  // TODO: these should be interlocked variables!
  stats_.pkt_rx += pkts;
  stats_.byt_rx += bytes;

  return pkts;
}


// Send the packet to its destination.
// Returns number of sucessfully sent packets.
int
Port_odp::send()
{
  // Check that this port is usable.
  if (config_.down)
    throw std::string("unable to send, port down");

  // Assemble some packets to send.
  // - Technically MAX_PKT_BURST isn't a real limit, but simplifies things for now
  int pkts = std::min((int)tx_queue_.size(), (int)MAX_PKT_BURST);
  odp_packet_t pkt_tbl[MAX_PKT_BURST];
  Context* pkt_tbl_cxt[MAX_PKT_BURST];  // need this to delete contexts after send
  int pkts_to_send = 0;

  // Send packets as long as the queue has work.
  uint64_t bytes = 0;
  for (int i = 0; i < pkts; i++) {
    // Get the next context, incr packet counter.
    Context* cxt = tx_queue_.front();
    tx_queue_.pop();

    // Check if packet is and ODP buffer.
    if (cxt->packet()->buf_dev_ != fp::FP_BUF_ODP) {
      printf("Not an ODP buffer, can't send out ODP port (unimplemented)\n");
      delete cxt->packet();
      delete cxt;
    }

    // Include packet in send batch.
    pkt_tbl[pkts_to_send] = (odp_packet_t)cxt->packet()->buf_handle_;
    pkt_tbl_cxt[pkts_to_send] = cxt;
    pkts_to_send++;
  }

  // Send batch of packets.
  // TODO: need to handle case when not all packets can be sent...
  int sent = odp_pktio_send(pktio_, pkt_tbl, pkts_to_send);
  if (sent < 0) {
    printf("Send failed!\n");
    sent = 0;
  }
  if (sent < pkts_to_send) {
    printf("Send failed to send all packets!\n");

    // Cleanup any ODP packet buffers that failed to send.
    odp_packet_free_multi(pkt_tbl+sent, pkts_to_send - sent);
  }

  // Cleanup all Contexts after send.
  for (int i = 0; i < pkts_to_send; i++) {
    Context* cxt = pkt_tbl_cxt[i];
    delete cxt->packet();
    delete cxt;
  }

  // Update port stats.
  // TODO: these should be interlocked variables!
  stats_.pkt_tx += sent;
  stats_.byt_tx += bytes; // TODO: not keeping track of this...

  return sent;
}

} // end namespace fp
