#include "port_odp.hpp"

#ifdef FP_ENABLE_ODP

#include "packet.hpp"
#include "context.hpp"

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
  auto name_off = args.find(';');

  std::string dev_name = args.substr(0, name_off);
  std::string name = args.substr(name_off + 1, args.length());

  // Sanity check length of device name.
  if (dev_name.length() < 1)
    throw std::string("bad device name");

  // Sanity check length of iternal port name.
  if (name.length() < 1)
    throw std::string("bad internal port name");

  // Set the port name.
  name_ = name;
  dev_name_ = dev_name;

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
  pktio_ = odp_pktio_open(dev_name.c_str(), pktpool_, &pktio_param);
  if (pktio_ == ODP_PKTIO_INVALID)
    throw std::string("Error: pktio create failed for dev: " + dev_name);
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
Port_odp::recv(Context* cxt)
{
//  constexpr int MAX = MAX_PKT_BURST;
  constexpr int MAX = 1;

  // Check if port is usable.
  if (config_.down)
    throw std::string("unable to recv, port down");

  // Recieve batch of packets.
  odp_packet_t pkt_tbl[MAX];
  int pkts = odp_pktio_recv(pktio_, pkt_tbl, MAX);
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
      std::cerr << "Unhandled: packet is segmented..." << std::endl;
    }
    uint8_t* seg_buf = static_cast<uint8_t*>(odp_packet_data(odp_pkt));
    uint32_t seg_len = odp_packet_len(odp_pkt);
    uint64_t arrival_ns = odp_time_to_ns(arrival);

    // Create a new packet and context associated with the packet.
    Packet* pkt = new Packet(seg_buf, seg_len, timespec{arrival.tv_sec, arrival.tv_nsec},
                             odp_pkt, fp::Buffer::BUF_TYPE::FP_BUF_ODP);
//    Context* cxt = new Context(pkt, id_, id_, 0, 0, 0);
    cxt->packet_ = pkt;
    cxt->input_.in_port  = cxt->input_.in_phy_port = id_;

    // Find reserved location in packet buffer.
    ///Context* cxt = reinterpret_cast<Context*>(odp_packet_user_area(odp_pkt));
    ///Packet* pkt = reinterpret_cast<Packet*>(cxt + sizeof(Context));

    // In-place construct, but not allocate Packet and Context.
    ///new (pkt) Packet(seg_buf, seg_len, arrival_ns, odp_pkt, fp::Buffer::BUF_TYPE::FP_BUF_ODP);
    ///new (cxt) Context(pkt, id_, id_, 0, 0, 0);

    // Sum up stats for all recieved packets.
    bytes += seg_len;
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
Port_odp::send(Context* ctx)
{
  // Check that this port is usable.
  if (config_.down)
    throw std::string("unable to send, port down");

  // Assemble some packets to send.
  // - Technically MAX_PKT_BURST isn't a real limit, but simplifies things for now
  int pkts = std::min((int)tx_queue_.size(), (int)MAX_PKT_BURST);
  odp_packet_t pkt_tbl[MAX_PKT_BURST];
  int pkts_to_send = 0;

  // Send packets as long as the queue has work.
  uint64_t bytes = 0;
  for (int i = 0; i < pkts; i++) {
    // Get the next context, inc packet counter.
    Context* cxt = tx_queue_.front();
    tx_queue_.pop();

    // Check if packet is an ODP buffer.
    if (cxt->packet().type() != fp::Buffer::BUF_TYPE::FP_BUF_ODP) {
      std::cerr << "Not an ODP buffer, can't send out ODP port (unimplemented)" << std::endl;
//      delete cxt->packet();
      delete cxt;
    }

    // Include packet in send batch.
    pkt_tbl[pkts_to_send] = (odp_packet_t)cxt->packet().buf_handle();
    // Destruct Packet and Context prior to send (handing ownership to ODP)
//    delete cxt->packet();
    delete cxt;
    pkts_to_send++;
  }

  // Send batch of packets.
  // TODO: need to handle case when not all packets can be sent...
  int sent = 0;
  if (pkts_to_send > 0) {
    sent = odp_pktio_send(pktio_, pkt_tbl, pkts_to_send);
    if (sent < 0) {
      std::cerr << "Send failed!" << std::endl;
      sent = 0;
    }
    if (sent < pkts_to_send) {
      std::cerr<< "Send failed to send all packets!" << std::endl;
      odp_packet_free_multi(pkt_tbl + sent, pkts_to_send - sent);
    }
  }

  // Update port stats.
  // TODO: these should be interlocked variables!
  stats_.pkt_tx += sent;
  stats_.byt_tx += bytes; // TODO: not keeping track of this...

  return sent;
}

} // end namespace fp

#endif // FP_ENABLE_ODP