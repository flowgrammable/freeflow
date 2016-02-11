#include "port_odp.hpp"
#include "packet.hpp"
#include "context.hpp"

//#include <arpa/inet.h>
//#include <netinet/in.h>
#include <stdio.h>
#include <cstring>

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
    throw std::string("Error: failed to lookup pktpool by name: " + PKT_POOL_NAME);

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
    throw std::string("Error: unable to start pktio dev: " + pktio_);
  return ret;
}


// Stop recieving and transmitting.
int Port_odp::close()
{
  int ret = odp_pktio_stop(pktio_);
  // TODO: how should port close error be reported?
  if (ret != 0)
    throw std::string("Error: unable to stop pktio dev: " + pktio_);
  return ret;
}


// Read packets from the socket.
// Returns number of sucessfully received packets.
int
Port_odp::recv()
{
  // Check if port is usable.
  if (config_.down)
    throw std::string("Port down");

  // Recieve batch of packets.
  odp_packet_t pkt_tbl[MAX_PKT_BURST];
  int pkts = odp_pktio_recv(pktio_, pkt_tbl, MAX_PKT_BURST);
  if (pkts < 0)
    throw std::string("Error: unable to recv on pktio dev: " + pktio_);

  // Wrap packets in packet context.
  int bytes = 0;
  for (int i = 0; i < pkts; i++) {
    odp_packet_t& pkt = pkt_tbl[i];

    // Create a new packet, leaving the data in the original buffer.
    Packet* pkt = packet_create(/*stuff goes here*/);

    // Create a new context associated with the packet.
    Context* cxt = new Context(pkt, id_, id_, 0, max_headers, max_fields);

    // Send context to pipeline.
    thread_pool.app()->lib().pipeline(cxt);
  }

  // TODO: finish this...


  //// UDP's Mechanism to read packets ////
  // Copy the buffer so that we guarantee that we don't
  // accidentally overwrite it when we have multiple
  // readers.
  //
  // FIXED: fp::Buffer copies the data.
  //
  // TODO: We should probably have a better buffer management
  // framework so that we don't have to copy each time we
  // create a packet.
  //
  // TODO: We should call functions which ask the application
  // for the maximum desired number of headers and fields
  // that can be extracted so we can produce a context
  // which takes up a minimal amount of space.
  // And probably move these to become global values instead
  // of locals to reduce function calls.
  //
  // NOTE: This should be done in the config() function.

  // Update port stats.
  // TODO: these should be interlocked stistics!
  stats_.pkt_rx += num_msg;
  stats_.byt_rx += bytes;

  return pkts;
}


// Send the packet to its destination.
// Returns number of sucessfully sent packets.
int
Port_odp::send()
{
  // TODO: finish this...

  //// UDP's Mechanism to read packets ////
  // Check that this port is usable.
  if (config_.down)
    throw std::string("port down");
  uint64_t bytes = 0;
  uint64_t pkts = tx_queue_.size();
  // Send packets as long as the queue has work.
  for (int i = 0; i < pkts; i++) {
    // Get the next context, incr packet counter.
    Context* cxt = tx_queue_.front();

    // Send the packet data associated with the context.
    //
    // TODO: Change this to use sendmmsg, or maybe decide based on
    // the size of the queue?
    long res = sendto(fd_, cxt->packet()->data(), cxt->packet()->size(),
      0, (struct sockaddr*)&dst_addr_, sizeof(dst_addr_));

    // Error check, else update num bytes sent.
    if (res < 0) {
      perror(std::string("port[" + std::to_string(id_) + "] sendto").c_str());
    }
    else
      bytes += res;

    // Release resources.
    delete cxt->packet();
    delete cxt;
    tx_queue_.pop();
  }

  // Update port stats.
  stats_.pkt_tx += pkts;
  stats_.byt_tx += bytes;

  return pkts;
}

} // end namespace fp
