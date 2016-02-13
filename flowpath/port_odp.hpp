#ifndef FP_PORT_ODP_HPP
#define FP_PORT_ODP_HPP

#include "port.hpp"

#include <string>

// Open Data Plane:
#include <odp.h>  // extern "C" protection provided

namespace fp
{

// ODP API Tuning Constants:
const char PKT_POOL_NAME[] = "packet_pool"; // TODO: factor out constant packet pool!
const int MAX_PKT_BURST = 16;  // max number of packets to recv() at once
const int SHM_PKT_POOL_BUF_SIZE = 1856;  // size (bytes) of each packet pool buffer
const int SHM_PKT_POOL_SIZE = 512*2048;  // size (bytes) of the entire packet pool

//const int UDP_BUF_SIZE = 2048;
//const int UDP_MSG_SIZE = 128;

class Port_odp : public Port
{
public:
  using Port::Port;

  // Constructors/Destructor.
  Port_odp(Port::Id, std::string const&);
  ~Port_odp();

  // Packet related funtions.
  int recv();
  int send();

  // Port state functions.
  int open();
  int close();

private:
  // Data members.
  odp_pktio_t pktio_; // ODP pktio device id (handle)
  odp_pool_t pktpool_; // ODP packet pool device id (handle)
  std::string dev_name_; // device name of port (pktio)
  //odp_packet_t pkt_tbl_[MAX_PKT_BURST];


  // Socket addresses.
  //struct sockaddr_in src_addr_;
  //struct sockaddr_in dst_addr_;

  // IO Buffers.
  //struct mmsghdr  messages_[UDP_BUF_SIZE];
  //struct iovec    iovecs_[UDP_BUF_SIZE];
  //char            buffer_[UDP_BUF_SIZE][UDP_MSG_SIZE];
  //struct timespec timeout_;
};

} // end namespace fp

#endif
