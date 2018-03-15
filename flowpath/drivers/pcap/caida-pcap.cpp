#include "system.hpp"
#include "dataplane.hpp"
#include "application.hpp"
#include "port_table.hpp"
#include "port.hpp"
#include "port_pcap.hpp"
#include "context.hpp"
///#include "packet.hpp" // temporary for sizeof packet
///#include "context.hpp" // temporary for sizeof context

#include <string>
#include <iostream>
#include <stdexcept>
#include <iomanip>
#include <signal.h>
#include <type_traits>
#include <bitset>
#include <map>
#include <tuple>
#include <functional>
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>

#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <typeinfo>

#define DEBUG 1

using namespace std;

static bool running;

void
sig_handle(int sig)
{
  running = false;
}

// Common types with guaranteed widths and signed-ness
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;
union u128_struct {
  u64 word[2];
  struct {
    u64 upper;
    u64 lower;
  };
};
using u128 = u128_struct;

// Ethernet EthertVLANVfsfype Constants:
constexpr u16 ETHTYPE_VLAN = 0x8100;
constexpr u16 ETHTYPE_IPV4 = 0x0800;
constexpr u16 ETHTYPE_IPV6 = 0x86DD;
// IPV4 Protocol Constants:
constexpr u8 IP_PROTO_ICMP = 0x01; // 1
constexpr u8 IP_PROTO_ICMPv6 = 0x3a; // 58
constexpr u8 IP_PROTO_TCP = 0x06; // 6
constexpr u8 IP_PROTO_UDP = 0x11; // 17
constexpr u8 IP_PROTO_IPSEC_ESP = 0x32; // 50
constexpr u8 IP_PROTO_IPSEC_AH = 0x33; // 51
constexpr u8 IP_PROTO_ENCAP_IPV6 = 0x29; //41

// Network byte-order translations
// - messy run-time translations...  need to fix with LLVM intrinsic
bool littleEndian;
inline void endianTest() {
  volatile u16 endianTest = 0xABCD;
  u8 end = *((volatile u8*)(&endianTest));
  littleEndian = (end == 0xCD);
}
u16 betoh16(u16 n) {
  if (littleEndian)
    return (n & 0x00FFU)<<8 | (n & 0xFF00U)>>8;
  else
    return n;
}
u32 betoh32(u32 n) {
  if (littleEndian)
    return (n & 0x000000FFUl)<<24 | (n & 0xFF000000Ul)>>24 |
           (n & 0x0000FF00Ul)<<8  | (n & 0x00FF0000Ul)>>8;
  else
    return n;
}
u64 betoh64(u64 n) {
  if (littleEndian)
    return (n&0x00000000000000FFUll)<<56 | (n&0x000000000000FF00Ull)<<40 |
           (n&0x0000000000FF0000Ull)<<24 | (n&0x00000000FF000000Ull)<<8  |
           (n&0x000000FF00000000Ull)>>8  | (n&0x0000FF0000000000Ull)>>24 |
           (n&0x00FF000000000000Ull)>>40 | (n&0xFF00000000000000Ull)>>56;
  else
    return n;
}

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

  if (lhs.tv_nsec < rhs.tv_nsec) {
    auto carry = (rhs.tv_nsec - lhs.tv_nsec) / NS_IN_SEC + 1;
    result.tv_nsec -= NS_IN_SEC * carry;
    result.tv_sec += carry;
  }
  if (lhs.tv_nsec - rhs.tv_nsec > NS_IN_SEC) {
    auto carry = (lhs.tv_nsec - rhs.tv_nsec) / NS_IN_SEC;
    result.tv_nsec += NS_IN_SEC * carry;
    result.tv_sec -= carry;
  }

  /* Return 1 if result is negative. */
//  return lhs.tv_sec < rhs.tv_sec;
  return result;
}

struct Fields {
 u64 ethSrc;
 u64 ethDst;
 u16 ethType;
 u16 vlanID;
 u32 ipv4Src;
 u32 ipv4Dst;
 u8  ipProto;
 u16 ipFlags;
 u16 srcPort;
 u16 dstPort;
};
struct FlowKey {
  u32 ipv4Src;
  u32 ipv4Dst;
  u16 srcPort;
  u16 dstPort;
  u8  ipProto;
};

using IpTuple = std::tuple<u32, u32>;
using PortTuple = std::tuple<u16, u16>;
using FlowKeyTuple = std::tuple<IpTuple, PortTuple, u8>;

class FlowRecord {
public:
  FlowRecord(u64 flowID, const u16 bytes, const timespec& ts) :
    flowID_(flowID), start_(ts),
    arrival_ns_({0}), bytes_({bytes}),
    saw_open_(false), saw_close_(false) { }

  u64 update(const u16 bytes, const timespec& ts) {
    bytes_.push_back(bytes);

    timespec diff = ts - start_; // NEED TO VERIFY
    u64 diff_ns = diff.tv_sec * 1000000000 + diff.tv_nsec;
    arrival_ns_.push_back(diff_ns);

    return flowID_;
  }

  u64 getFlowID() const { return flowID_; }
  size_t packets() const { return arrival_ns_.size(); }
  u64 bytes() const { return std::accumulate(bytes_.begin(), bytes_.end(), u64(0)); }

  const vector<u64>& getArrivalV() const { return arrival_ns_; }
  const vector<u16>& getByteV() const { return bytes_; }

private:
  u64 flowID_;
  timespec start_;
  vector<u64> arrival_ns_;
  vector<u16> bytes_;
  bool saw_open_;  // change to direction open/close?
  bool saw_close_;
};

template <typename R>
static constexpr typename enable_if< is_integral<R>::value, R >::type
bitmask(const size_t bitCount)
{
//  return (onecount != 0)
//      ? (static_cast<R>(-1) >> ((sizeof(R) * CHAR_BIT) - onecount))
//      : 0;
    return static_cast<R>(-(bitCount != 0))
        & (static_cast<R>(-1) >> ((sizeof(R) * CHAR_BIT) - bitCount));
}
template <typename R>
static constexpr typename enable_if< is_integral<R>::value, R >::type
bytemask(const size_t byteCount)
{
    return static_cast<R>(-(byteCount != 0))
        & (static_cast<R>(-1) >> (sizeof(R) - byteCount));
}


// Keeps track of safe range of access within packet.
class View {
public:
  View(uint8_t* ptr, size_t bytes) :
    begin_(ptr), end_(ptr+bytes),
    beginCP_(begin_), endCP_(end_),
    beginAbs_(begin_), endAbs_(end_) {
  }
  View(uint8_t* ptr, size_t bytes, size_t orig) :
    begin_(ptr), end_(ptr+bytes),
    beginCP_(begin_), endCP_(end_),
    beginAbs_(begin_), endAbs_(ptr+orig) {
  }

  bool sufficient(size_t bytes) {
    u8* end = static_cast<u8*>(begin_) + bytes;
    return end <= end_;
  }
  template<typename R, size_t bytes = sizeof(R)>
  bool sufficient() {
    return sufficient(bytes);
  }

  void discard(size_t bytes) {
    if (!sufficient(bytes))
      throw std::out_of_range("Insufficient bytes to discard");

    constrain(bytes);
  }

  void discardEnd(size_t bytes) {
    if (!sufficient(bytes))
      throw std::out_of_range("Insufficient bytes to discard");

    constrainEnd(bytes);
  }

  template<typename R, size_t bytes = sizeof(R)>
  R peek() {
    if (!sufficient(bytes))
      throw std::out_of_range("Insufficient bytes to read");

    R val = read<R,bytes>();
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  R peekEnd() {
    if (!sufficient(bytes))
      throw std::out_of_range("Insufficient bytes to read");

    R val = readEnd<R,bytes>();
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  R get() {
    if (!sufficient(bytes))
      throw std::out_of_range("Insufficient bytes to read");

    R val = read<R,bytes>();
    constrain(bytes);
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  R getEnd() {
    if (!sufficient(bytes))
      throw std::out_of_range("Insufficient bytes to read");

    R val = readEnd<R,bytes>();
    constrain(bytes);
    return val;
  }

  // Remaining bytes in View
  template<typename R>
  typename enable_if< is_integral<R>::value, R >::type
  bytes() {
    R bytes = static_cast<u8*>(end_) - static_cast<u8*>(begin_);
    return bytes;
  }

  // Remaining bytes in committed view (checkpoint)
  template<typename R>
  typename enable_if< is_integral<R>::value, R >::type
  committedBytes() {
    R bytes = static_cast<u8*>(endCP_) - static_cast<u8*>(beginCP_);
    return bytes;
  }

  // Total bytes in original view
  template<typename R>
  typename enable_if< is_integral<R>::value, R >::type
  absoluteBytes() {
    R bytes = static_cast<const u8*>(endAbs_) - static_cast<const u8*>(beginAbs_);
    return bytes;
  }

  // Bytes moved in current view since last commit.
  template<typename R>
  typename enable_if< is_integral<R>::value, R >::type
  pendingBytes() {
    R bytes = this->committedBytes<R>() - this->bytes<R>();
    return bytes;
  }

  void commit() {
    beginCP_ = begin_;
    endCP_ = end_;
  }

  void rollback() {
    begin_ = beginCP_;
    end_ = endCP_;
  }

  void revert() {
    begin_ = beginAbs_;
    end_ = endAbs_;
  }

private:
  void constrain(size_t bytes) {
    u8* pos = static_cast<u8*>(begin_) + bytes;
    begin_ = pos;
  }

  void constrainEnd(size_t bytes) {
    u8* pos = static_cast<u8*>(end_) - bytes;
    end_ = pos;
  }

  template<typename R, size_t bytes = sizeof(R)>
  typename enable_if< is_integral<R>::value, R >::type
  read() {
    R* ptr = static_cast<R*>(begin_);
    R val = *ptr & bytemask<R>(bytes);
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  typename enable_if< is_compound<R>::value, R >::type
  read() {
    R* ptr = static_cast<R*>(begin_);
    R val = *ptr;
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  typename enable_if< is_integral<R>::value, R >::type
  readEnd() {
    R* ptr = static_cast<R*>(end_) - 1; // read one element (R) back from end...
    R val = *ptr & bytemask<R>(bytes);
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  typename enable_if< is_compound<R>::value, R >::type
  readEnd() {
    R* ptr = static_cast<R*>(end_) - 1; // read one element (R) back from end...
    R val = *ptr;
    return val;
  }

private:
  // Current View:
  void* begin_;
  void* end_; // one past end
  // Checkpoint View:
  void* beginCP_;
  void* endCP_; // one past end
  // Absolute View:
  void *const beginAbs_;
  void *const endAbs_;  // one past end
};


class EvalContext {
public:
  View v;
  u16 origBytes;
  Fields k;
  stringstream extractLog;

  EvalContext(fp::Packet* p) :
    v(p->data(), p->size()), k{}, extractLog()
  {
    // Replace View with observed bytes
    if (p->type() == fp::Buffer::FP_BUF_PCAP) {
      const auto& buf = static_cast<const fp::Buffer::Pcap&>( p->buf_object() );
//      v = View(p->data(), p->size(), buf->wire_bytes_);
      origBytes = buf.wire_bytes_;
    }
  }
};

enum HeaderType {UNKNOWN = 0, ETHERNET, IP, IPV4, IPV6, TCP, UDP};

int extract(EvalContext&, HeaderType);
int extract_ethernet(EvalContext&);
int extract_ip(EvalContext&);
int extract_ipv4(EvalContext&);
int extract_ipv6(EvalContext&);
int extract_tcp(EvalContext&);
int extract_udp(EvalContext&);
int extract_icmpv4(EvalContext&);
int extract_icmpv6(EvalContext&);
int extract_ipsec_esp(EvalContext&);
int extract_ipsec_ah(EvalContext&);

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
  Fields& gKey = cxt.k;
  stringstream& log = cxt.extractLog;

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
    log << ": Unknown ethertype=" << etherType << '\n';
    return extracted;
  }
  if (status == 0) { return extracted; }
  extracted += status;
  ipProto = gKey.ipProto;

  { // check for IP fragmentation
    stringstream fragLog;
    fragLog << hex << setfill('0') << setw(2);
    constexpr u16 DF = 0x4000;
    constexpr u16 MF = 0x2000;
    constexpr u16 FragOffset = 0x1FFF;
    u16 frag = gKey.ipFlags & (MF | FragOffset);
    if (frag != 0) {
      // Packet was fragmented...
      if (frag == MF) {
        // First fragment, parse L4
        fragLog << "First IPv4 fragment! flags=0x" << gKey.ipFlags << '\n';
      }
      else if ((~frag & MF) == MF) {
        fragLog << "Last IPv4 fragment! flags=0x" << gKey.ipFlags << '\n';
        return extracted;
      }
      else {
        fragLog << "Another IPv4 fragment! flags=0x" << gKey.ipFlags << '\n';
        return extracted;
      }
    }
    log << fragLog.str();
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
    status = extract_tcp(cxt);
    break;
  case IP_PROTO_UDP:
    status = extract_udp(cxt);
    break;
  case IP_PROTO_IPSEC_AH:
    status = extract_ipsec_ah(cxt);
    break;
  case IP_PROTO_IPSEC_ESP:
    status = extract_ipsec_esp(cxt);
    break;
  case IP_PROTO_ENCAP_IPV6:
    status = extract_ipv6(cxt);
    if (status > 0) {
      ipProto = gKey.ipProto;
      goto L4;
    }
    break;
  default:
    log << __func__ << ": Unknown ipProto=" << ipProto << '\n';
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
  Fields& gKey = cxt.k;
  stringstream& log = cxt.extractLog;

  log << '(' << __func__ << ")\n";

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
  log << __func__ << ": extracted " << extracted << "B\n";
  view.commit();  // success
  return extracted;
}

//// Extract IP ////
int
extract_ip(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.k;
  stringstream& log = cxt.extractLog;

  log << '(' << __func__ << ")\n";

  u8 ipVersion = view.peek<u8>() >> 4;

  switch (ipVersion) {
  case 4:
    return extract_ipv4(cxt);
  case 6:
//    return extract_ipv6(cxt);
    log << __func__ << ": Skipping IPv6...\n";
  default:
    log << __func__ << ": Abort, Unknown IP Version! ipVersion=" << ipVersion << '\n';
    return 0;
  }
}

//// Extract IPv4 ////
int
extract_ipv4(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.k;
  stringstream& log = cxt.extractLog;

  log << '(' << __func__ << ")\n";

  auto tmp = view.get<u8>();
  u8 ipVersion = (tmp & 0xF0)>>4;
  u8 ihl = tmp & 0x0F;  // number of 32-bit words in header
  if (ipVersion != 4) {
    log << __func__ << ": Abort, Not IPv4! ipVersion=" << ipVersion << '\n';
    view.rollback();
    return 0;
  }

  // check ihl field for consistency
  // min: 5
  // max: remaining payload (view)
  if (ihl < 5 || ihl*4 > view.committedBytes<size_t>()) {
    log << __func__ << ": Warn IPv4! ihl=" << static_cast<int>(ihl) << '\n';
//    return view.pendingBytes<int>();
  }

  //uint8_t dscp = (0xFC & *v.get())>>2;
  //uint8_t ecn = 0x02 & *v.get();
  view.discard(1);

  auto ipv4Length = betoh16(view.get<u16>());		// bytes in packet including ipv4 header

  // check ipv4Length field for consistency, should exactly match view
  if (ipv4Length != view.committedBytes<size_t>()) {
    log << __func__ << ": Warn IPv4! ipv4Length=" << ipv4Length << '\n';
//    return 0; // malformed ipv4
  }

  //uint16_t id = bs16(*v.get());
  view.discard(2);

  //bool df = (0x40 & *v.get()) == 0x40;
  //bool mf = (0x20 & *v.get()) == 0x20;
  //uint16_t frag_offset = 0x1FFF & bs16(*(uint16_t*)v.get());	// 64-bit block offset relative to original
  u16 flags = gKey.ipFlags = betoh16(view.get<u16>());
  bool df = (flags & 0x4000) != 0;
  bool mf = (flags & 0x2000) != 0;
  u16 fragOffset = flags & 0x1FFF;  // offset in 8B blocks

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
  log << __func__ << ": extracted " << extracted << "B\n";
  view.commit();
  return extracted;
}

//// Extract IPv6 ////
int
extract_ipv6(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.k;
  stringstream& log = cxt.extractLog;

  log << '(' << __func__ << ")\n";

  // Assert sufficient bytes in view for minimum IPv6 frame...
  {
    size_t bytes = view.committedBytes<size_t>();
    if (bytes < 40) {
      log << __func__ << ": View insufficient for IPv6 Header ("
          << bytes << " / 40 B)\n";
      // IMPROVE: extract some bytes even though truncated?...
//      return view.pendingBytes<int>();
    }
  }

  auto tmp = view.get<u8>();
  u8 ipVersion = (tmp & 0xF0) >> 4;
  if (ipVersion != 6) {
    log << __func__ << ": Abort IPv6! ipVersion=" << ipVersion << '\n';
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
    log << __func__ << ": Warning IPv6! payloadLength=" << payloadLength << '\n';
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
  log << __func__ << ": extracted " << extracted << "B\n";
  view.commit();
  return extracted;
}

//// Extract TCP ////
int
extract_tcp(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.k;
  stringstream& log = cxt.extractLog;

  log << '(' << __func__ << ")\n";

  gKey.srcPort = betoh16(view.get<u16>());
  gKey.dstPort = betoh16(view.get<u16>());

  //uint32_t seq_num = bs32(*(uint32_t*)v.get());
  view.discard(4);

  //uint32_t ack_num = bs32(*(uint32_t*)v.get());
  view.discard(4);

  // {offset, flags}
  u8 offset = (0xF0 & view.get<u8>())>>4;	// size of TCP header in 4B words
  view.discard(1);
  // END flags

  // check TCP header offset is consistant with packet size
  if (offset < 4) {
    log << __func__ << ": Warn TCP header length too small! offset=" << int(offset) << '\n';
//    return 0;  // Malformed TCP
  }
  if (offset*4 > view.committedBytes<size_t>()) {
    log << __func__ << ": Warn TCP header length larger than buf! offset=" << int(offset) << '\n';
  }

  //uint16_t window = bs16(*(uint16_t*)v.get());
  view.discard(2);
  //uint16_t checksum = bs16(*(uint16_t*)v.get());
  view.discard(2);
  //uint16_t urg_num = bs16(*(uint16_t*)v.get());
  view.discard(2);

  // restrict view before extracting TCP options:
  auto extracted = view.pendingBytes<int>();
  log << __func__ << ": extracted " << extracted << "B\n";
  view.commit();

  // BEGIN TCP options
  if (offset > 5) {
    log << __func__ << ": Ignoring TCP options present (TODO: Fixme)\n";
    // handle TCP options as needed...
    int skip = (offset - 5) * 4;
    if (skip <= view.bytes<int>()) {
      log << __func__ << ": TCP options " << skip << '/' << view.bytes<int>() << '\n';
      view.discard(skip); // skip past...

      // restrict view after exctracting TCP options:
      extracted += view.pendingBytes<int>();
      log << __func__ << ": options extracted " << extracted << "B\n";
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
  Fields& gKey = cxt.k;
  stringstream& log = cxt.extractLog;

  log << '(' << __func__ << ")\n";

  gKey.srcPort = betoh16(view.get<u16>());
  gKey.dstPort = betoh16(view.get<u16>());

  u16 udpLen = betoh16(view.get<u16>());  // length (in bytes) of UDP header and Payload
  if (udpLen < 8) {
    log << __func__ << ": Warn UDP header length too small! udpLen=" << udpLen << '\n';
//    return 0;  // Malformed UDP
  }
  if (udpLen > view.committedBytes<size_t>()) {
    log << __func__ << ": Warn UDP header length larger than buf! udpLen=" << udpLen << '\n';
  }

  //uint16_t checksum = bs16(*(uint16_t*)v.get());
  view.discard(2);

  // restrict view after extracting TCP:
  auto extracted = view.pendingBytes<int>();
  log << __func__ << ": extracted " << extracted << "B\n";
  view.commit();
  return extracted;
}


//// Extract IPsec AH ////
int
extract_ipsec_ah(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.k;
  stringstream& log = cxt.extractLog;

  log << '(' << __func__ << ")\n";

  // Assert sufficient bytes in view for minimum ESP frame...
  {
    size_t bytes = view.committedBytes<size_t>();
    if (bytes < 12) {
      log << __func__ << ": Warn buffer insufficient for AH Header ("
          << bytes << " / 12 B)\n";
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
  log << __func__ << ": extracted " << extracted << "B\n";
  view.commit();
  return extracted;
}


//// Extract IPsec ESP ////
int
extract_ipsec_esp(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.k;
  stringstream& log = cxt.extractLog;

  log << '(' << __func__ << ")\n";

  // Assert sufficient bytes in view for minimum ESP frame...
  {
    size_t bytes = view.committedBytes<size_t>();
    if (bytes < 8) {
      log << __func__ << ": Warn buffer insufficient for ESP Header ("
          << bytes << " / 8 B)\n";
      // IMPROVE: extract some bytes even though truncated?...
//      return 0; // length of ESP insufficient
    }
  }

  auto spi = betoh32(view.get<u32>());
  auto seq = betoh32(view.get<u32>());

  // restrict view after extracting TCP:
  auto extracted = view.pendingBytes<int>();
  log << __func__ << ": extracted " << extracted << "B\n";
  view.commit();
  return extracted;
}


//// Extract ICMPv4 ////
int
extract_icmpv4(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.k;
  stringstream& log = cxt.extractLog;

  log << '(' << __func__ << ")\n";

  // Assert sufficient bytes in view for minimum ESP frame...
  {
    size_t bytes = view.committedBytes<size_t>();
    if (bytes < 4) {
      log << __func__ << ": Warn buffer insufficient for ICMPv4 Header ("
          << bytes << " / 4 B)\n";
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
  log << __func__ << ": extracted " << extracted << "B\n";
  view.commit();
  return extracted;
}


//// Extract ICMPv6 ////
int
extract_icmpv6(EvalContext& cxt) {
  View& view = cxt.v;
  Fields& gKey = cxt.k;
  stringstream& log = cxt.extractLog;

  log << '(' << __func__ << ")\n";

  // Assert sufficient bytes in view for minimum ESP frame...
  {
    size_t bytes = view.committedBytes<size_t>();
    if (bytes < 4) {
      log << __func__ << ": Warn buffer insufficient for ICMPv6 Header ("
          << bytes << " / 4 B)\n";
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
  log << __func__ << ": extracted " << extracted << "B\n";
  view.commit();
  return extracted;
}

std::ostream& printEval(std::ostream &out, EvalContext& evalCxt) {
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
  out << evalCxt.extractLog.str();
  return out;
}

bool timespec_less(const timespec& lhs, const timespec& rhs) {
  return (lhs.tv_sec < rhs.tv_sec) &&
         (lhs.tv_nsec < rhs.tv_nsec);
}

bool timespec_greater(const timespec& lhs, const timespec& rhs) {
  return (lhs.tv_sec > rhs.tv_sec) &&
         (lhs.tv_nsec > rhs.tv_nsec);
}

bool timespec_equal(const timespec& lhs, const timespec& rhs) {
  return (lhs.tv_sec == rhs.tv_sec) &&
         (lhs.tv_nsec == rhs.tv_nsec);
}

struct context_cmp {
  bool operator()(const fp::Context& lhs, const fp::Context& rhs) {
    const timespec& lhs_ts = lhs.packet().timestamp();
    const timespec& rhs_ts = rhs.packet().timestamp();

    return timespec_greater(lhs_ts, rhs_ts);
  }
};

class caidaHandler {
public:
  caidaHandler() = default;
  ~caidaHandler() = default;

private:
  int advance(int id) {
    fp::Context cxt;
    if (pcap_.at(id).recv(&cxt) > 0) {
      next_cxt_.push( std::move(cxt) );
      return 1; // 1 packet read
    }
    else {
      if (pcap_files_.at(id).size() > 0) {
        // Queue next pcap file and prepare for first read:
        auto& list = pcap_files_.at(id);
        string file = list.front();
        list.pop();

        cerr << "Opening PCAP file: " << file << endl;
        pcap_.erase(id);
        pcap_.emplace(std::piecewise_construct,
                      std::forward_as_tuple(id),
                      std::forward_as_tuple(id, file.c_str()) );

        return advance(id); // recursive call to handle 1st read
      }
    }
    cerr << "No packets to read from Port: " << id << endl;
    return 0; // no packets to read
  }

public:
  void open_list(int id, string listFile) {
    // Open text list file:
    ifstream lf;
    lf.open(listFile);
    if (!lf.is_open()) {
      cerr << "Failed to open_list(" << id << ", " << listFile << endl;
      return;
    }

    // Find directory component of listFile:
    size_t path = listFile.find_last_of('/');

    // Queue all files up in list (in read-in order)
    auto& fileQueue = pcap_files_[id];
    string fileName;
    while (getline(lf, fileName)) {
      fileName.insert(0, listFile, 0, path+1);
      cerr << fileName << endl;
      fileQueue.emplace( std::move(fileName) );
    }
    lf.close();

    // Attempt to open first file in stream:
    fileName = std::move(fileQueue.front());
    fileQueue.pop();
    open(id, fileName);
  }

  void open(int id, string file) {
    // Automatically call open_list if file ends in ".files":
    const string ext(".files");
    if (file.compare(file.length()-ext.length(), string::npos, ext) == 0) {
      return open_list(id, file);
    }

    // Open PCAP file:
    pcap_.emplace(std::piecewise_construct,
                  std::forward_as_tuple(id),
                  std::forward_as_tuple(id, file.c_str()) );
    pcap_files_[id];  // ensure id exists

    // Attempt to queue first packet in stream:
    advance(id);
  }

  fp::Context recv() {
    if (next_cxt_.empty()) {
      for (const auto& l : pcap_files_) {
        // Sanity check to ensure no dangling unread pcap files...
        assert(l.second.size() == 0);
      }
      return fp::Context();
    }

    // Swap priority_queue container objects without a built-in move optimization:
    fp::Context top = std::move( const_cast<fp::Context&>(next_cxt_.top()) );
    next_cxt_.pop();
    int id = top.in_port();

    // Queue next packet in stream:
    advance(id);
    return top;
  }

  int rebound(fp::Context* cxt) {
    return pcap_.at(cxt->in_port()).rebound(cxt);
  }

private:
  // Next packet in stream from each portID, 'sorted' by arrival timestamp:
  priority_queue<fp::Context, vector<fp::Context>, context_cmp> next_cxt_;
  // Currently open pcap file hand by portID:
  map<int, fp::Port_pcap> pcap_;
  // Next pcap files to open by portID:
  map<int, std::queue<std::string>> pcap_files_;
};

int
main(int argc, const char* argv[])
{
  signal(SIGINT, sig_handle);
  signal(SIGKILL, sig_handle);
  signal(SIGHUP, sig_handle);
  running = true;

  if (argc < 2 || argc > 3) {
    cout << "Usage: " << argv[0] <<
            " (directionA.pcap) [directionB.pcap]" << endl;
    return 2;
  }

  // Open Pcap Files:
  caidaHandler caida;
  cout << argv[1] << endl;
  caida.open(1, argv[1]);
  if (argc == 3) {
    cout << argv[2] << endl;
    caida.open(2, argv[2]);
  }

  // TODO: factor me out
  endianTest();

  // Metadata structures:
//  using test = decltype(std::make_tuple(k.ipv4Src, k.ipv4Dst, k.ipProto, k.srcPort, k.dstPort));
  u64 flowID = 0;
  unordered_map<string, FlowRecord> flows;

  // Global Stats:
  u16 maxPacketSize = std::numeric_limits<u16>::lowest();
  u16 minPacketSize = std::numeric_limits<u16>::max();

  // File read:
  s64 count = 0;
  fp::Context cxt = caida.recv();
  while (cxt.packet_ != nullptr) {
    fp::Packet* p = cxt.packet_;
    EvalContext evalCxt(p);

    // Dump hex of packet:
    const int capBytes = p->size();
    count++;
    stringstream pktDump;
    {
    uint8_t *const d = p->data();
    pktDump << hex << setfill('0');
    for (int i = 0; i < capBytes; ++i) {
      pktDump << setw(2) << static_cast<int>(d[i]);
    }
    }

    // Attempt to parse packet headers:
    try {
    int status = extract(evalCxt, IP);
    }
    catch (std::exception& e) {
      cerr << count << " (" << capBytes << "B): ";
      cerr << evalCxt.v.absoluteBytes<int>() << ", "
           << evalCxt.v.committedBytes<int>() << ", "
           << evalCxt.v.pendingBytes<int>() << '\n';
      cerr << pktDump.str() << '\n';
      printEval(cerr, evalCxt);
      cerr << "> Exception occured durring to extract: " << e.what() << endl;
    }

    // Associate packet with flow:
    Fields& k = evalCxt.k;
//    auto t = std::make_tuple(k.ipv4Src, k.ipv4Dst, k.ipProto, k.srcPort, k.dstPort);
    FlowKey fk {k.ipv4Src, k.ipv4Dst, k.srcPort, k.dstPort, k.ipProto};
    char* fkp = reinterpret_cast<char*>(&fk);
    string fks(fkp, sizeof(fk));

//    FlowRecord fr(flowID, bytes, p->timestamp());
//    auto status = flows.insert(make_pair(fks, fr));
    u16 wireBytes = evalCxt.origBytes;
    maxPacketSize = std::max(maxPacketSize, wireBytes);
    minPacketSize = std::min(minPacketSize, wireBytes);
    auto status = flows.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(fks),
                                 std::forward_as_tuple(flowID, wireBytes, p->timestamp()));
    // Unexpectingly, emplace construsts a record before testing lookup...
    if (!status.second) {
      auto id = (*status.first).second.update(wireBytes, p->timestamp());
      evalCxt.extractLog << "Existing FlowID: " << id << '\n';
    }
    else {
      evalCxt.extractLog << "New FlowID: " << flowID << '\n';
      flowID++;
    }

/*
    // Print packet info:
//    printf("ethType: %x\n"\
//           "ethSrc: %llx\n"\
//           "ethDst: %llx\n"\
//           "ipv4Src: %lx\n"\
//           "ipv4Dst: %lx\n"\
//           "ipProto: %x\n"\
//           "srcPort: %i\n"\
//           "dstPort: %i\n",
//           k.ethType,
//           k.ethSrc,
//           k.ethDst,
//           k.ipv4Src,
//           k.ipv4Dst,
//           k.ipProto,
//           k.srcPort,
//           k.dstPort
//    );
*/

    caida.rebound(&cxt);
    cxt = caida.recv();
  }

  // Print Flows:
  u64 totalBytes = 0;
  size_t totalPackets = 0;

  // Sort Flows by Bytes/Packets:
  multimap<u64, const FlowRecord*> sorted_bytes;
  multimap<size_t, const FlowRecord*> sorted_packets;
  for (const auto& e : flows) {
    const FlowRecord& flow = e.second;
    auto packets = flow.packets();
    auto bytes = flow.bytes();
    totalPackets += packets;
    totalBytes += bytes;
    sorted_packets.insert(make_pair(packets, &flow));
    sorted_bytes.insert(make_pair(bytes, &flow));
//    const auto&& max = std::max_element(flow.getByteV().cbegin(), flow.getByteV().cend());
  }
  for (const auto& e : sorted_packets) {
    const FlowRecord& flow = *(e.second);
    auto pkts = flow.packets();
    auto byts = flow.bytes();
    cout << "FlowID=" << flow.getFlowID()
         << ", packets=" << pkts
         << ", bytes=" << byts
         << ", ppBytes=" << byts/pkts
         << '\n';
  }

  // Print global stats:
  cout << "Max packet size: " << maxPacketSize << '\n';
  cout << "Min packet size: " << minPacketSize << '\n';
  cout << "Total bytes: " << totalBytes << '\n';
  cout << "Total packets: " << totalPackets << '\n';

  return 0;
}


