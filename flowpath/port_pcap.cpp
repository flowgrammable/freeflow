#include "port_pcap.hpp"

#include "packet.hpp"
#include "context.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>

#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <iostream>

#include <exception>

extern "C" {
#include <zlib.h>
}

namespace entangle {

FILE *gzip_open(const char *path, const char *mode) {
  FILE *fp = NULL;
  gzFile z = gzopen(path, mode);
  if (z == NULL) {
    perror("gzopen");
    return NULL;
  }

  static const cookie_io_functions_t gzip_io_func = {
    .read = gzip_read,
    .write = gzip_write,
    .seek = gzip_seek,
    .close = gzip_close
  };

  fp = fopencookie(z, mode, gzip_io_func);
  if (fp == NULL) {
    perror("fopencookie");
    gzclose(z);
    return NULL;
  }

  return fp;
}

extern "C" {
// ZLIB (GZIP) Custom Stream Hook Functions:
// https://www.gnu.org/software/libc/manual/html_node/Hook-Functions.html
ssize_t gzip_read(void *cookie, char *buffer, size_t size) {
  return gzread(static_cast<gzFile>(cookie), buffer, size);
}

ssize_t gzip_write(void *cookie, const char *buffer, size_t size) {
  return gzwrite(static_cast<gzFile>(cookie), buffer, size);
}

int gzip_seek(void *cookie, off64_t *position, int whence) {
  return gzseek(static_cast<gzFile>(cookie), *position, whence);
}

int gzip_close(void *cookie) {
  return gzclose(static_cast<gzFile>(cookie));
}
} // END extern "C" (ZLIB Custom Stream)
} // END entangle namespace



namespace fp
{
// dirA pcap -> Port 1
// dirB pcap -> Port 2
// time sync / offset?
//


// ODP Port constructor. Parses the device name from
// the input string given, allocates a new internal ID.
Port_pcap::Port_pcap(Port::Id id, std::string const& args)
  : Port(id, ""),
    handle_(args.c_str())
{
//  auto name_off = args.find(' ');
//  std::string dirA = args.substr(0, name_off);
//  handles_.emplace_back(dirA);

//  if (name_off != std::string::npos) {
//    std::string dirB = args.substr(name_off+1, std::string::npos);
//    handles_.emplace_back(dirB);
//  }

  /*
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
//  odp_pktio_param_t pktio_param;
//  odp_pktio_param_init(&pktio_param);

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
*/
}


// Start recieving and transmitting.
int
Port_pcap::open()
{
  throw std::string("open not defined");
//  int ret = odp_pktio_start(pktio_);
//  // TODO: how should port open error be reported?
//  if (ret != 0)
//    throw std::string("Error: unable to start pktio dev");
//  return ret;
}


// Stop recieving and transmitting.
int
Port_pcap::close()
{
    throw std::string("close not defined");
//  int ret = odp_pktio_stop(pktio_);
//  // TODO: how should port close error be reported?
//  if (ret != 0)
//    throw std::string("Error: unable to stop pktio dev");
//  return ret;
}

// Returns / deallocates buffers related to Context / Packet
int
Port_pcap::rebound(Context* cxt) {
  delete cxt->packet_;
  cxt->packet_ = nullptr;
  return 0;
}


// Read packets from the socket.
// Returns number of sucessfully received packets.
int
Port_pcap::recv(Context* cxt)
{
  int status = handle_.next();
  if (status != 1)
    return 0;

  timespec arrival_ts = handle_.get_ts();
//  uint64_t arrival_ns = arrival_ts.tv_sec * 1000000000 +
//                        arrival_ts.tv_nsec;
  const uint8_t* seg_buf = handle_.peek_buf();
  auto meta = handle_.peek_meta();
  int seg_len = meta->caplen;
  int orig_len = meta->len;

  // Create a new packet and context associated with the packet.
  Packet* pkt = new Packet(seg_buf, seg_len, arrival_ts, &handle_, fp::Buffer::BUF_TYPE::FP_BUF_PCAP);
  // Hack to set wire bytes in buffer without reworking specalization...
  static_cast<fp::Buffer::Pcap&>(
   const_cast<fp::Buffer::Base&>(pkt->buf_object()) ).wire_bytes_ = orig_len;
  cxt->packet_ = pkt;
  cxt->input_.in_port = cxt->input_.in_phy_port = id_;

  // Update port stats.
  // TODO: these should be interlocked variables!
  stats_.pkt_rx += 1;
  stats_.byt_rx += orig_len;

  return 1; // 1 recieved packet

/*
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
    Packet* pkt = new Packet(seg_buf, seg_len, arrival_ns, odp_pkt, fp::Buffer::BUF_TYPE::FP_BUF_ODP);
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
  */
}


// Send the packet to its destination.
// Returns number of sucessfully sent packets.
int
Port_pcap::send(Context* ctx)
{
    throw std::string("send not defined");
  /*
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
  */
}

} // end namespace fp
