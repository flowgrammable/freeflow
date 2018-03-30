#ifndef DRIVER_CAIDA_PCAP_HPP
#define DRIVER_CAIDA_PCAP_HPP

#include "system.hpp"
#include "dataplane.hpp"
#include "application.hpp"
#include "port_table.hpp"
#include "port.hpp"
#include "port_pcap.hpp"
#include "context.hpp"
///#include "packet.hpp" // temporary for sizeof packet
///#include "context.hpp" // temporary for sizeof context
#include "util_bitmask.hpp"

#include <string>
#include <iostream>
#include <stdexcept>
#include <iomanip>
#include <ctime>
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
#include <chrono>

#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <endian.h>

#include <typeinfo>

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


// Definition of useful Flags/Metadata
// - Union of all possible flags our huristic cares about...
// - Flags are not limited to header bits, but can also be metadata...
enum class ProtoFlags {
  isUDP =1<<3,
  isTCP =1<<2,
  isIPv6=1<<1,
  isIPv4=1<<0
};
ENABLE_BITMASK_OPERATORS(ProtoFlags);

// Offsets starts relative to {Flags, Fragment Offset} in IPv4 Header
enum class IPFlags {
  DF=1<<1,
  MF=1<<0
};
ENABLE_BITMASK_OPERATORS(IPFlags);

enum class TCPFlags {
  NS =1<<8,
  CWR=1<<7,
  ECE=1<<6,
  URG=1<<5,
  ACK=1<<4,
  PSH=1<<3,
  RST=1<<2,
  SYN=1<<1,
  FIN=1<<0
};
ENABLE_BITMASK_OPERATORS(TCPFlags);

struct Fields {
  ProtoFlags fProto;
  IPFlags fIP;
  u16 ipFragOffset;
  TCPFlags fTCP;
  u64 ethSrc;
  u64 ethDst;
  u16 ethType;
  u16 vlanID;
  u32 ipv4Src;
  u32 ipv4Dst;
  u8  ipProto;
//  u16 ipFlags;  // and IP FragOffset
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

class EvalContext;  // forward declaration

class FlowRecord {
public:
  FlowRecord(u64 flowID, const u16 bytes, const timespec& ts) :
    flowID_(flowID), start_(ts),
    arrival_ns_({0}), bytes_({bytes}),
    saw_open_(false), saw_close_(false) {
  }

  u64 update(const EvalContext& e);
  u64 update(const u16 bytes, const timespec& ts);

  u64 getFlowID() const { return flowID_; }
  size_t packets() const { return arrival_ns_.size(); }
  u64 bytes() const { return std::accumulate(bytes_.begin(), bytes_.end(), u64(0)); }

  timespec last() const {
    // TODO: replace this with chrono duration...
    timespec t = start_;
    auto dif = arrival_ns_.back();
    t.tv_nsec += dif % 1000000000;
    t.tv_sec += dif / 1000000000;
    if (t.tv_nsec >= 1000000000) {
      t.tv_sec += t.tv_nsec / 1000000000;
      t.tv_nsec %= 1000000000;
    }
    return t;
  }

  const std::vector<u64>& getArrivalV() const { return arrival_ns_; }
  const std::vector<u16>& getByteV() const { return bytes_; }

private:
  u64 flowID_;
  // Timeseries:
  timespec start_;
  std::vector<u64> arrival_ns_;
  std::vector<u16> bytes_;
  // Flow State:
  bool saw_open_;  // TODO: change to direction open/close?
  bool saw_close_;
};


template <typename R>
static constexpr typename std::enable_if< std::is_integral<R>::value, R >::type
bitmask(const size_t bitCount)
{
    return static_cast<R>(-(bitCount != 0))
        & (static_cast<R>(-1) >> ((sizeof(R) * CHAR_BIT) - bitCount));
}
template <typename R>
static constexpr typename std::enable_if< std::is_integral<R>::value, R >::type
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

  bool sufficient(size_t bytes) const {
    u8* end = static_cast<u8*>(begin_) + bytes;
    return end <= end_;
  }
  template<typename R, size_t bytes = sizeof(R)>
  bool sufficient() const {
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
  R peek() const {
    if (!sufficient(bytes))
      throw std::out_of_range("Insufficient bytes to read");

    R val = read<R,bytes>();
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  R peekEnd() const {
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
  typename std::enable_if< std::is_integral<R>::value, R >::type
  bytes() const {
    R bytes = static_cast<u8*>(end_) - static_cast<u8*>(begin_);
    return bytes;
  }

  // Remaining bytes in committed view (checkpoint)
  template<typename R>
  typename std::enable_if< std::is_integral<R>::value, R >::type
  committedBytes() const {
    R bytes = static_cast<u8*>(endCP_) - static_cast<u8*>(beginCP_);
    return bytes;
  }

  // Total bytes in original view
  template<typename R>
  typename std::enable_if< std::is_integral<R>::value, R >::type
  absoluteBytes() const {
    R bytes = static_cast<const u8*>(endAbs_) - static_cast<const u8*>(beginAbs_);
    return bytes;
  }

  // Bytes moved in current view since last commit.
  template<typename R>
  typename std::enable_if< std::is_integral<R>::value, R >::type
  pendingBytes() const {
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
  typename std::enable_if< std::is_integral<R>::value, R >::type
  read() const {
    R* ptr = static_cast<R*>(begin_);
    R val = *ptr & bytemask<R>(bytes);
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  typename std::enable_if< std::is_compound<R>::value, R >::type
  read() const {
    R* ptr = static_cast<R*>(begin_);
    R val = *ptr;
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  typename std::enable_if< std::is_integral<R>::value, R >::type
  readEnd() const {
    R* ptr = static_cast<R*>(end_) - 1; // read one element (R) back from end...
    R val = *ptr & bytemask<R>(bytes);
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  typename std::enable_if< std::is_compound<R>::value, R >::type
  readEnd() const {
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


class EvalContext {
public:
  View v;
  u16 origBytes;
  Fields k;
  std::stringstream extractLog;

  EvalContext(fp::Packet* const p);
  const fp::Packet& packet() const {return pkt_;}

private:
  const fp::Packet& pkt_;
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



class caidaHandler {
public:
  caidaHandler() = default;
  ~caidaHandler() = default;

private:
  int advance(int id);

public:
  void open_list(int id, std::string listFile);
  void open(int id, std::string file);

  fp::Context recv();
  int rebound(fp::Context* cxt);

private:
  // Next packet in stream from each portID, 'sorted' by arrival timestamp:
  std::priority_queue<fp::Context, std::deque<fp::Context>, context_cmp> next_cxt_;
  // Currently open pcap file hand by portID:
  std::map<int, fp::Port_pcap> pcap_;
  // Next pcap files to open by portID:
  std::map<int, std::queue<std::string>> pcap_files_;
};


#endif
