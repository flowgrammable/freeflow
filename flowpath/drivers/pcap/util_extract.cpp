#include "util_extract.hpp"

#include <string>
#include <iostream>
#include <stdexcept>
#include <iomanip>
#include <ctime>
#include <signal.h>
#include <type_traits>
#include <bitset>
#include <map>
#include <functional>
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <chrono>

#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <boost/endian/conversion.hpp>

using namespace std;
using namespace util_view;


// Network byte-order translations
// - compile time swap; need to fix with LLVM intrinsic
constexpr bool littleEndian = boost::endian::order::native == boost::endian::order::little;

static inline u16 betoh16(u16 n) {
  return littleEndian ? boost::endian::endian_reverse(n) : n;
}
static inline u32 betoh32(u32 n) {
  return littleEndian ? boost::endian::endian_reverse(n) : n;
}
static inline u64 betoh64(u64 n) {
  return littleEndian ? boost::endian::endian_reverse(n) : n;
}

template <typename T>
inline T betoh(T n) {
//  return boost::endian::big_to_native(n);
  return littleEndian ? boost::endian::endian_reverse(n) : n;
}
template <typename T>
inline T htobe(T n) {
//  return boost::endian::native_to_big(n);
  return littleEndian ? boost::endian::endian_reverse(n) : n;
}


// TODO: replace this with chrono duration...
timespec
operator-(const timespec& lhs, const timespec& rhs) {
  constexpr auto NS_IN_SEC = 1000000000LL;
  timespec result;

  /* Compute the time. */
  result.tv_sec = lhs.tv_sec - rhs.tv_sec;
  result.tv_nsec = lhs.tv_nsec - rhs.tv_nsec;

  /* Perform the carry. */
//  if (result.tv_nsec > NS_IN_SEC) {
//   result.tv_sec++;
//   result.tv_nsec -= NS_IN_SEC;
//  }

//  if (lhs.tv_nsec < rhs.tv_nsec) {
//    auto carry = (rhs.tv_nsec - lhs.tv_nsec) / NS_IN_SEC + 1;
//    result.tv_nsec -= NS_IN_SEC * carry;
//    result.tv_sec += carry;
//  }
//  if (lhs.tv_nsec - rhs.tv_nsec > NS_IN_SEC) {
//    auto carry = (lhs.tv_nsec - rhs.tv_nsec) / NS_IN_SEC;
//    result.tv_nsec += NS_IN_SEC * carry;
//    result.tv_sec -= carry;
//  }

  if (result.tv_nsec < 0) {
    auto carry = result.tv_nsec / NS_IN_SEC + 1;
    result.tv_nsec += NS_IN_SEC * carry;
    result.tv_sec -= carry;
  }
  else if (result.tv_nsec >= NS_IN_SEC) {
    auto carry = result.tv_nsec / NS_IN_SEC;
    result.tv_nsec -= NS_IN_SEC * carry;
    result.tv_sec += carry;
  }

  return result;
}

std::string print_ip(uint32_t ip) {
  const std::string SEP(".");
  std::string ips = std::to_string(ip >> 24);
  ip &= 0x00FFFFFF;
  ips.append( SEP + std::to_string(ip >> 16) );
  ip &= 0x0000FFFF;
  ips.append( SEP + std::to_string(ip >> 8) );
  ip &= 0x000000FF;
  ips.append( SEP + std::to_string(ip) );
  return ips;
}

std::string print_flow_key_string(const Fields& k) {
  const std::string SEP(",");
  std::string fks = print_ip(k.ipv4Src);
  fks.append( SEP + print_ip(k.ipv4Dst) );
  fks.append( SEP + std::to_string(k.srcPort) );
  fks.append( SEP + std::to_string(k.dstPort) );
  fks.append( SEP + std::to_string(uint16_t(k.ipProto)) );
  return fks;
}

std::string print_flow_key_string(const FlowKeyTuple& k) {
  const std::string SEP(",");

  const Ipv4Tuple& addrTuple = std::get<0>(k);
  const PortTuple& portTuple = std::get<1>(k);

  std::string fks = print_ip(std::get<0>(addrTuple));
  fks.append( SEP + print_ip(std::get<1>(addrTuple)) );
  fks.append( SEP + std::to_string(std::get<0>(portTuple)) );
  fks.append( SEP + std::to_string(std::get<1>(portTuple)) );
  fks.append( SEP + std::to_string(uint16_t(std::get<2>(k))) );
  return fks;
}

std::string make_flow_key_string(const Fields& k) {
  struct __attribute__((packed)) FlowKeyStruct{
    u32 ipv4Src;
    u32 ipv4Dst;
    u16 srcPort;
    u16 dstPort;
    u8 ipProto;
  };
  FlowKeyStruct fkt {k.ipv4Src, k.ipv4Dst, k.srcPort, k.dstPort, k.ipProto};
  static_assert(sizeof(FlowKeyStruct) == 13, "FlowKeyStruct appears to be padded.");

  std::string fks(reinterpret_cast<const char*>(&fkt), sizeof(fkt));
  assert(fks.size() == 13);
  return fks;
}

FlowKeyTuple make_flow_key_tuple(const Fields& k) {
  Ipv4Tuple ipt {k.ipv4Src, k.ipv4Dst};
  PortTuple pt {k.srcPort, k.dstPort};
  FlowKeyTuple fkt {move(ipt), move(pt), k.ipProto};
  return fkt;
}

Flags make_flags_bitset(const Fields& k) {
  return (static_cast<Flags>(k.fProto) << 11) |
         (static_cast<Flags>(k.fIP) << 9) |
          static_cast<Flags>(k.fTCP);
}


u64 FlowRecord::update(const EvalContext& e) {
  // TODO: record flowstate...
  const auto& protoFlags = e.fields.fProto;
  const auto& ipFlags = e.fields.fIP;
  const auto& tcpFlags = e.fields.fTCP;
  const int payloadBytes = static_cast<int>(e.origBytes) - e.v.committedBytes<int>();
  // Committed bytes is insufficient...

  // First Update
  if (arrival_ns_ts_.size() == 0) {
    protoFlags_ = protoFlags;
  }

  // Track TCP state:
  if (protoFlags & ProtoFlags::isTCP) {
    // TCP Session Changes:
    if (tcpFlags & TCPFlags::SYN) {
    //if (tcpFlags == TCPFlags::SYN) {
      // Detect if initator vs. responder
      saw_open_ = true;
    }
    else if (tcpFlags & TCPFlags::FIN) {
      // Detect if terminator vs. term. responder
      saw_close_ = true;
    }
    else if (tcpFlags & TCPFlags::RST) {
      // Detect if initates reset
      saw_reset_ = true;
    }

    // TCP Flow Updates:
    if (tcpFlags & TCPFlags::ACK) {
      // Acknowledge present
      ACK_count_++;
      if (payloadBytes > 0) {
        directionality_++;
      }
      else if (payloadBytes == 0) {
        directionality_--;
      }
      else {
        // negative
        cerr << "error in payload size for directionality: " << payloadBytes << endl;
      }
    }
    if (tcpFlags & TCPFlags::URG) {
      // Urgent pointer is significant (application specific)
      URG_count_++;
    }
    if (tcpFlags & TCPFlags::PSH) {
      // Push flag (application specific)
      PSH_count_++;
    }

    // TCP Congestion Control Information:
    // e.g. NS, CWR, ECE

    // TCP Flow State Update:
    last_seq_ = e.fields.tcpSeqNum;
  }

  // Update flow processing log:
#ifdef DEBUG_LOG
  log_.append(e.extractLog.str());
#endif

  // TODO: record flowstate...
  return FlowRecord::update(e.origBytes, e.packet().timestamp());
}

u64 FlowRecord::update(const u16 bytes, const timespec& ts) {
  constexpr auto NS_IN_SEC = 1000000000LL;
  pkts_++;
  bytes_ += bytes;

  timespec diff = ts - start_; // NEED TO VERIFY
  u64 diff_ns = diff.tv_sec * NS_IN_SEC + diff.tv_nsec;

  if (MODE_ == MODE_EN::TIMESERIES) {
    // Keep timeseries of all packets:
    byte_ts_.push_back(bytes);
    arrival_ns_ts_.push_back(diff_ns);
  }
  else {
    // Only keep time of last packet:
    arrival_ns_ts_.assign(size_t(1), diff_ns);
  }
  return flowID_;
}


EvalContext::EvalContext(fp::Packet* const p) :
  v(p->data(), p->size()), fields{}, pkt_(*p)
{
  // Replace View with observed bytes
  if (p->type() == fp::Buffer::FP_BUF_PCAP) {
    const auto& buf = static_cast<const fp::Buffer::Pcap&>( p->buf_object() );
//      v = View(p->data(), p->size(), buf->wire_bytes_);
    origBytes = buf.wire_bytes_;
  }
}

EvalContext::EvalContext(const fp::Packet& p) :
  v(p.data(), p.size()), fields{}, pkt_(p)
{
  // Replace View with observed bytes
  if (p.type() == fp::Buffer::FP_BUF_PCAP) {
    const auto& buf = static_cast<const fp::Buffer::Pcap&>( p.buf_object() );
//      v = View(p->data(), p->size(), buf->wire_bytes_);
    origBytes = buf.wire_bytes_;
  }
}



// function to extract packet into global key
// Arguments:
// - pkt: pointer to a linear, virtually contiguous packet
// - pktLen: length of packet (in bytes); max is implicitly 64kB
// Returns by Argument Manipulation:
// - view: indicating the end of the last extracted header.  Indicates
//   start of effective payload (if one exists, be sure to check pktLen)
// - gKey: extracted fields to key
int
extract(EvalContext& cxt, HeaderType t = ETHERNET) {
  // provides a view of packet payload:
  View& view = cxt.v;
  Fields& gKey = cxt.fields;
#ifdef DEBUG_LOG
  stringstream& log = cxt.extractLog;
#endif

  int extracted = 0;
  int status;

  u16 etherType = 0;
  u8 ipProto = 0;

  // Override layer processing:
  switch (t) {
  case ETHERNET:
     goto L2;
    break;
  case IP:
  case IPV4:
    etherType = ETHTYPE_IPV4;
    goto L3;
    break;
  case IPV6:
    etherType = ETHTYPE_IPV6;
    goto L3;
    break;
  case TCP:
    ipProto = 6;
    goto L4;
    break;
  case UDP:
    ipProto = 17;
    goto L4;
    break;
  default:
    return extracted;
  }

L2:
  // Process L2:
  status = extract_ethernet(cxt);
  extracted += status;
  etherType = gKey.ethType;

L3:
  // Process L3:
  switch (etherType) {
  case ETHTYPE_IPV4:
  case ETHTYPE_IPV6:
    status = extract_ip(cxt);
    break;
  default:
#ifdef DEBUG_LOG
    log << __func__ << ": Unknown ethertype=" << etherType << '\n';
#endif
    return extracted;
  }
  if (status == 0) { return extracted; }
  extracted += status;
  ipProto = gKey.ipProto;

  { // check for IP fragmentation
#ifdef DEBUG_LOG
    stringstream fragLog;
    fragLog << hex << setfill('0') << setw(2);
#endif

    if (gKey.fIP & IPFlags::MF || gKey.ipFragOffset != 0) {
      // Either MF set, or FragOffset non-0; packet is a fragment...
      if (gKey.fIP & IPFlags::MF && gKey.ipFragOffset == 0) {
        // First fragment, parse L4
#ifdef DEBUG_LOG
        fragLog << "First IPv4 fragment!\n";
#endif
      }
      else if (gKey.fIP & IPFlags::MF) {
#ifdef DEBUG_LOG
        fragLog << "Another IPv4 fragment! ipFragOffset=0x"
                << gKey.ipFragOffset << '\n';
#endif
        return extracted;
      }
      else {
#ifdef DEBUG_LOG
        fragLog << "Last IPv4 fragment! ipFragOffset=0x"
                << gKey.ipFragOffset << '\n';
#endif
        return extracted;
      }
    }
#ifdef DEBUG_LOG
    log << fragLog.str();
#endif
  }

L4:
  // Process L4:
  switch (ipProto) {
  case IP_PROTO_ICMP:
    status = extract_icmpv4(cxt);
    break;
  case IP_PROTO_ICMPv6:
    status = extract_icmpv6(cxt);
    break;
  case IP_PROTO_TCP:
    gKey.fProto |= ProtoFlags::isTCP;
    status = extract_tcp(cxt);
    break;
  case IP_PROTO_UDP:
    gKey.fProto |= ProtoFlags::isUDP;
    status = extract_udp(cxt);
    break;
  case IP_PROTO_IPSEC_AH:
    status = extract_ipsec_ah(cxt);
    break;
  case IP_PROTO_IPSEC_ESP:
    status = extract_ipsec_esp(cxt);
    break;
  case IP_PROTO_ENCAP_IPV6:
    // Diabled IPv6...
//    status = extract_ipv6(cxt);
//    if (status > 0) {
//      ipProto = gKey.ipProto;
//      goto L4;
//    }
    break;
  default:
#ifdef DEBUG_LOG
    log << __func__ << ": Unknown ipProto=" << ipProto << '\n';
#endif
    return extracted;
  }
//  if (status == 0) { return extracted; }
  extracted += status;

  // success
  return extracted;
}


//// Extract Ethernet ////
int
extract_ethernet(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.fields;
#ifdef DEBUG_LOG
  stringstream& log = cxt.extractLog;
#endif

//  log << '(' << __func__ << ")\n";

  gKey.ethDst = betoh64(view.get<u64,6>());
  gKey.ethSrc = betoh64(view.get<u64,6>());

  u16 etherType = betoh16(view.get<u16>());

  // grab frame check sequence at end of packet
  u32 ethCRC = betoh32(view.getEnd<u32>());

  // extract outermost VLAN frame:
  if (etherType == ETHTYPE_VLAN) {
    //uint8_t priority = (0xE0 & *v.get())>>5;
    //bool dei = (0x10 & *v.get()) == 0x10;
    gKey.vlanID = betoh16(view.get<u16>()) & 0x1FFF;
    etherType = betoh16(view.get<u16>());

    // skip any remaining VLAN tags
    while (etherType == ETHTYPE_VLAN) {
      view.discard(2);
      etherType = betoh16(view.get<u16>());
    }
  }
  gKey.ethType = etherType;  // finally resolved ethertype

  // restrict view after extracting ethernet + [VLAN/s]:
  auto extracted = view.pendingBytes<int>();

#ifdef DEBUG_LOG
  log << __func__ << ": extracted " << extracted << "B\n";
#endif
  view.commit();  // success
  return extracted;
}


//// Extract IP ////
int
extract_ip(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.fields;
#ifdef DEBUG_LOG
  stringstream& log = cxt.extractLog;
#endif


//  log << '(' << __func__ << ")\n";

  u8 ipVersion = view.peek<u8>() >> 4;

  switch (ipVersion) {
  case 4:
    gKey.fProto |= ProtoFlags::isIPv4;
    return extract_ipv4(cxt);
  case 6:
    gKey.fProto |= ProtoFlags::isIPv6;
//    return extract_ipv6(cxt);
#ifdef DEBUG_LOG
    log << __func__ << ": Skipping IPv6...\n";
#endif
  default:
#ifdef DEBUG_LOG
    log << __func__ << ": Abort, Unknown IP Version! ipVersion=" << ipVersion << '\n';
#endif
    return 0;
  }
}


//// Extract IPv4 ////
int
extract_ipv4(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.fields;
#ifdef DEBUG_LOG
  stringstream& log = cxt.extractLog;
#endif

//  log << '(' << __func__ << ")\n";

  auto tmp = view.get<u8>();
  u8 ipVersion = (tmp & 0xF0)>>4;
  u8 ihl = tmp & 0x0F;  // number of 32-bit words in header
  if (ipVersion != 4) {
#ifdef DEBUG_LOG
    log << __func__ << ": Abort, Not IPv4! ipVersion=" << ipVersion << '\n';
#endif
    view.rollback();
    return 0;
  }

  // check ihl field for consistency
  // min: 5
  // max: remaining payload (view)
  if (ihl < 5 || ihl*4 > view.committedBytes<size_t>()) {
#ifdef DEBUG_LOG
    log << __func__ << ": Warn IPv4! ihl=" << static_cast<int>(ihl) << '\n';
#endif
//    return view.pendingBytes<int>();
  }

  //uint8_t dscp = (0xFC & *v.get())>>2;
  //uint8_t ecn = 0x02 & *v.get();
  view.discard(1);

  auto ipv4Length = betoh16(view.get<u16>());		// bytes in packet including ipv4 header

  // check ipv4Length field for consistency, should exactly match view
  if (ipv4Length != view.committedBytes<size_t>()) {
#ifdef DEBUG_LOG
    log << __func__ << ": Warn IPv4! ipv4Length=" << ipv4Length << '\n';
#endif
//    return 0; // malformed ipv4
  }

  //uint16_t id = bs16(*v.get());
  view.discard(2);

  u16 flags = betoh16(view.get<u16>());
  gKey.fIP = static_cast<IPFlags>(flags >> 13);
  gKey.ipFragOffset = flags & 0x1FFF;  // offset in 8B blocks

  //uint8_t ttl = *v.get();
  view.discard(1);

  auto proto = view.get<u8>();
  gKey.ipProto = proto;

  //uint16_t checksum = bs16(*(uint16_t*)v.get());
  view.discard(2);

  gKey.ipv4Src = betoh32(view.get<u32>());
  gKey.ipv4Dst = betoh32(view.get<u32>());

  // BEGIN IPv4 options
  if (ihl > 5) {
    // do special IPv4 options handling here as needed
    view.discard((ihl-5) * 4); // skip past
  }
  // END IPv4 options

  // restrict view after extracting IPv4:
  auto extracted = view.pendingBytes<int>();
#ifdef DEBUG_LOG
  log << __func__ << ": extracted " << extracted << "B\n";
#endif
  view.commit();
  return extracted;
}


//// Extract IPv6 ////
int
extract_ipv6(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.fields;
#ifdef DEBUG_LOG
  stringstream& log = cxt.extractLog;
#endif

//  log << '(' << __func__ << ")\n";

  // Assert sufficient bytes in view for minimum IPv6 frame...
  {
    size_t bytes = view.committedBytes<size_t>();
    if (bytes < 40) {
#ifdef DEBUG_LOG
      log << __func__ << ": View insufficient for IPv6 Header ("
          << bytes << " / 40 B)\n";
#endif
      // IMPROVE: extract some bytes even though truncated?...
//      return view.pendingBytes<int>();
    }
  }

  auto tmp = view.get<u8>();
  u8 ipVersion = (tmp & 0xF0) >> 4;
  if (ipVersion != 6) {
#ifdef DEBUG_LOG
    log << __func__ << ": Abort IPv6! ipVersion=" << ipVersion << '\n';
#endif
    view.rollback();
    return 0;
  }

  u8 tc = (tmp & 0x0F) << 4; // upper nibble of tc byte
  tmp = view.get<u8>();
  tc |= (tmp & 0xF0) >> 4;  // lower nibble of tc byte

  // Need to verify...  byteswap likely needed
  u32 flowLabel = u32(tmp & 0x0F) << 16 | betoh16(view.get<u32,2>());

  u16 payloadLength = betoh16(view.get<u16>());

  // check payload length for consistency
  // exception: 0 when jumbo extention is present...
  // otherwise: num bytes of payload including any options.
  if (payloadLength > view.committedBytes<size_t>()) {
#ifdef DEBUG_LOG
    log << __func__ << ": Warning IPv6! payloadLength=" << payloadLength << '\n';
#endif
//    return 0; // payload length of ipv6 insufficient
  }

//  gKey.ipProto = view.get<u8>();
  u8 nextHeader = view.get<u8>();
  gKey.ipProto = nextHeader;

  auto ttl = view.get<u8>();

  auto srcIP = view.get<u128>();
  auto dstIP = view.get<u128>();

  /// FIXME!
  // BEGIN IPv6 options
//  if (nextHeader) {
    // follow IPv6 option chain as needed
    // - all extention headers are 8 bytes in size
//    view.discard(8); // skip past
//  }
  // END IPv6 options
  // last 'nextHeader' indicates payload protocol
//  gKey.ipProto = nextHeader;

  // restrict view after extracting IPv4:
  auto extracted = view.pendingBytes<int>();
#ifdef DEBUG_LOG
  log << __func__ << ": extracted " << extracted << "B\n";
#endif
  view.commit();
  return extracted;
}


//// Extract TCP ////
int
extract_tcp(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.fields;
#ifdef DEBUG_LOG
  stringstream& log = cxt.extractLog;
#endif

//  log << '(' << __func__ << ")\n";

  gKey.srcPort = betoh16(view.get<u16>());
  gKey.dstPort = betoh16(view.get<u16>());

  gKey.tcpSeqNum = betoh32(view.get<u32>());
  gKey.tcpAckNum = betoh32(view.get<u32>());

  // {offset, flags}
  u16 tmp = view.get<u8>();
  u8 offset = (0xF0 & tmp)>>4;	// size of TCP header in 4B words
  tmp = ((tmp&0x0F) << 8) | view.get<u8>();
  gKey.fTCP = static_cast<TCPFlags>(tmp);
  // END flags

  // check TCP header offset is consistant with packet size
  if (offset < 4) {
#ifdef DEBUG_LOG
    log << __func__ << ": Warn TCP header length too small! offset=" << int(offset) << '\n';
#endif
//    return 0;  // Malformed TCP
  }
  if (offset*4 > view.committedBytes<size_t>()) {
#ifdef DEBUG_LOG
    log << __func__ << ": Warn TCP header length larger than buf! offset=" << int(offset) << '\n';
#endif
  }

  //uint16_t window = bs16(*(uint16_t*)v.get());
  view.discard(2);
  //uint16_t checksum = bs16(*(uint16_t*)v.get());
  view.discard(2);
  //uint16_t urg_num = bs16(*(uint16_t*)v.get());
  view.discard(2);

  // restrict view before extracting TCP options:
  auto extracted = view.pendingBytes<int>();
#ifdef DEBUG_LOG
  log << __func__ << ": extracted " << extracted << "B\n";
#endif
  view.commit();

  // BEGIN TCP options
  if (offset > 5) {
#ifdef DEBUG_LOG
    log << __func__ << ": Ignoring TCP options present (TODO: Fixme)\n";
#endif
    // handle TCP options as needed...
    int skip = (offset - 5) * 4;
    if (skip <= view.bytes<int>()) {
#ifdef DEBUG_LOG
      log << __func__ << ": TCP options " << skip << '/' << view.bytes<int>() << '\n';
#endif
      view.discard(skip); // skip past...

      // restrict view after exctracting TCP options:
      extracted += view.pendingBytes<int>();
#ifdef DEBUG_LOG
      log << __func__ << ": options extracted " << extracted << "B\n";
#endif
      view.commit();
    }
  }
  // END TCP options

  return extracted;
}


//// Extract UDP ////
int
extract_udp(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.fields;
#ifdef DEBUG_LOG
  stringstream& log = cxt.extractLog;
#endif

//  log << '(' << __func__ << ")\n";

  gKey.srcPort = betoh16(view.get<u16>());
  gKey.dstPort = betoh16(view.get<u16>());

  u16 udpLen = betoh16(view.get<u16>());  // length (in bytes) of UDP header and Payload
  if (udpLen < 8) {
#ifdef DEBUG_LOG
    log << __func__ << ": Warn UDP header length too small! udpLen=" << udpLen << '\n';
#endif
//    return 0;  // Malformed UDP
  }
  if (udpLen > view.committedBytes<size_t>()) {
#ifdef DEBUG_LOG
    log << __func__ << ": Warn UDP header length larger than buf! udpLen=" << udpLen << '\n';
#endif
  }

  //uint16_t checksum = bs16(*(uint16_t*)v.get());
  view.discard(2);

  // restrict view after extracting TCP:
  auto extracted = view.pendingBytes<int>();
#ifdef DEBUG_LOG
  log << __func__ << ": extracted " << extracted << "B\n";
#endif
  view.commit();
  return extracted;
}


//// Extract IPsec AH ////
int
extract_ipsec_ah(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.fields;
#ifdef DEBUG_LOG
  stringstream& log = cxt.extractLog;
#endif

//  log << '(' << __func__ << ")\n";

  // Assert sufficient bytes in view for minimum ESP frame...
  {
    size_t bytes = view.committedBytes<size_t>();
    if (bytes < 12) {
#ifdef DEBUG_LOG
      log << __func__ << ": Warn buffer insufficient for AH Header ("
          << bytes << " / 12 B)\n";
#endif
      // IMPROVE: extract some bytes even though truncated?...
//      return 0; // length of AH insufficient
    }
  }

  u8 nextHeader = view.get<u8>();
  gKey.ipProto = nextHeader;

  u8 payloadLen = view.get<u8>();

  // reserved
  view.discard(2);

  auto spi = betoh32(view.get<u32>());
  auto seq = betoh32(view.get<u32>());

  /// FIXME: is there any part of the ICV contained?

  // restrict view after extracting TCP:
  auto extracted = view.pendingBytes<int>();
#ifdef DEBUG_LOG
  log << __func__ << ": extracted " << extracted << "B\n";
#endif
  view.commit();
  return extracted;
}


//// Extract IPsec ESP ////
int
extract_ipsec_esp(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.fields;
#ifdef DEBUG_LOG
  stringstream& log = cxt.extractLog;
#endif

//  log << '(' << __func__ << ")\n";

  // Assert sufficient bytes in view for minimum ESP frame...
  {
    size_t bytes = view.committedBytes<size_t>();
    if (bytes < 8) {
#ifdef DEBUG_LOG
      log << __func__ << ": Warn buffer insufficient for ESP Header ("
          << bytes << " / 8 B)\n";
#endif
      // IMPROVE: extract some bytes even though truncated?...
//      return 0; // length of ESP insufficient
    }
  }

  auto spi = betoh32(view.get<u32>());
  auto seq = betoh32(view.get<u32>());

  // restrict view after extracting TCP:
  auto extracted = view.pendingBytes<int>();
#ifdef DEBUG_LOG
  log << __func__ << ": extracted " << extracted << "B\n";
#endif
  view.commit();
  return extracted;
}


//// Extract ICMPv4 ////
int
extract_icmpv4(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.fields;
#ifdef DEBUG_LOG
  stringstream& log = cxt.extractLog;
#endif

//  log << '(' << __func__ << ")\n";

  // Assert sufficient bytes in view for minimum ESP frame...
  {
    size_t bytes = view.committedBytes<size_t>();
    if (bytes < 4) {
#ifdef DEBUG_LOG
      log << __func__ << ": Warn buffer insufficient for ICMPv4 Header ("
          << bytes << " / 4 B)\n";
#endif
      // IMPROVE: extract some bytes even though truncated?...
//      return 0; // length of ICMPv4 insufficient
    }
  }

  auto type = view.get<u8>();
  auto code = view.get<u8>();
  auto checksum = betoh16(view.get<u16>());

  auto other = betoh32(view.get<u32>());

  // restrict view after extracting:
  auto extracted = view.pendingBytes<int>();
#ifdef DEBUG_LOG
  log << __func__ << ": extracted " << extracted << "B\n";
#endif
  view.commit();
  return extracted;
}


//// Extract ICMPv6 ////
int
extract_icmpv6(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.fields;
#ifdef DEBUG_LOG
  stringstream& log = cxt.extractLog;
#endif

//  log << '(' << __func__ << ")\n";

  // Assert sufficient bytes in view for minimum ESP frame...
  {
    size_t bytes = view.committedBytes<size_t>();
    if (bytes < 4) {
#ifdef DEBUG_LOG
      log << __func__ << ": Warn buffer insufficient for ICMPv6 Header ("
          << bytes << " / 4 B)\n";
#endif
      // IMPROVE: extract some bytes even though truncated?...
//      return 0; // length of ICMPv6 insufficient
    }
  }

  auto type = view.get<u8>();
  auto code = view.get<u8>();
  auto checksum = betoh16(view.get<u16>());

  auto other = betoh32(view.get<u32>());

  // commit view after success:
  auto extracted = view.pendingBytes<int>();
#ifdef DEBUG_LOG
  log << __func__ << ": extracted " << extracted << "B\n";
#endif
  view.commit();
  return extracted;
}

std::ostream& print_eval(std::ostream &out, const EvalContext& evalCxt) {
  int committedBytes = evalCxt.v.absoluteBytes<int>() - evalCxt.v.committedBytes<int>();
  int pendingBytes = evalCxt.v.committedBytes<int>();
  string committed("|"), pending("|");
  if (committedBytes > 0) {
    committed = string(committedBytes * 2, '+');
  }
  if (pendingBytes > 0) {
    pending = string(pendingBytes * 2, '-');
    pending.append("|");
  }
  out << committed << pending << '\n';
#ifdef DEBUG_LOG
  out << evalCxt.extractLog.str();
#endif
  return out;
}

stringstream hexdump(const void* msg, size_t bytes) {
  auto m = static_cast<const uint8_t*>(msg);
  stringstream ss;

  ss << std::hex << std::setfill('0');
  for (decltype(bytes) i = 0; i < bytes; i++) {
    ss << setw(2) << static_cast<unsigned int>(m[i]);
  }
  return ss;
}

void print_hex(const string& s) {
  stringstream ss = hexdump(s.c_str(), s.size());
  cerr << ss.str() << endl;
}

stringstream dump_packet(const fp::Packet& p) {
  return hexdump(p.data(), p.size());
}

void print_packet(const fp::Packet& p) {
  cerr << p.timestamp().tv_sec << ':' << p.timestamp().tv_nsec << '\n'
       << dump_packet(p).str() << endl;
}

stringstream print_flow(const FlowRecord& flow) {
  stringstream ss;

  // Print flow summary information:
  auto pkts = flow.packets();
  auto byts = flow.bytes();
  ss << "FlowID=" << flow.getFlowID()
     << ", key=" << 0
     << ", packets=" << pkts
     << ", bytes=" << byts
     << ", ppBytes=" << byts/pkts
     << ", " << (flow.sawSYN()?"SYN":"")
             << (flow.sawFIN()?"|FIN":"")
             << (flow.sawRST()?"|RST\n":"\n");
  return ss;
}

void print_log(const EvalContext& e) {
#ifdef DEBUG_LOG
  cerr << e.extractLog.str() << endl;
#endif
}

void print_log(const FlowRecord& flow) {
  cerr << flow.getLog() << endl;
}

