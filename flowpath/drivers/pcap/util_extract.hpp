#ifndef DRIVER_UTIL_EXTRACT_HPP
#define DRIVER_UTIL_EXTRACT_HPP

#include "util_bitmask.hpp"

#include "system.hpp"
//#include "dataplane.hpp"
//#include "application.hpp"
//#include "port_table.hpp"
//#include "port.hpp"
#include "port_pcap.hpp"
#include "context.hpp"

#include "util_view.hpp"

//#include "absl/utility/utility.h"
#include <utility>
#include <type_traits>


//#define DEBUG_LOG 1


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


timespec operator-(const timespec& lhs, const timespec& rhs);


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


using Ipv4Tuple = std::tuple<u32, u32>; // src
using PortTuple = std::tuple<u16, u16>;
using FlowKeyTuple = std::tuple<Ipv4Tuple, PortTuple, u8>;
//using Flags = std::bitset<15>;
using Flags = uint16_t;

struct Fields {
  // Interpreted Flags:
  ProtoFlags fProto;
  IPFlags fIP;
  u16 ipFragOffset;
  TCPFlags fTCP;

  // Raw Fields:
  u64 ethSrc;
  u64 ethDst;
  u16 ethType;
  u16 vlanID;
  u32 ipv4Src;
  u32 ipv4Dst;
  u8  ipProto;
  u16 srcPort;
  u16 dstPort;
};

std::string print_ip(uint32_t);
std::string print_flow_key_string(const Fields&);
std::string make_flow_key_string(const Fields&);
FlowKeyTuple make_flow_key_tuple(const Fields&);
Flags make_flags_bitset(const Fields&);

// Sanity checks:
static_assert(sizeof(Flags) == 2, "Flags is not 2B?");
static_assert(!std::is_integral<FlowKeyTuple>::value, "FKT is integral?!");
//static_assert(std::has_unique_object_representations_v<Ipv4Tuple>, "Ipv4Tuple has padding!");
static_assert(std::has_unique_object_representations_v<TCPFlags>, "TCPFlags has padding!");

// Compile-time tuple detection:
template<typename> struct is_tuple: std::false_type {};
template<typename ...T> struct is_tuple<std::tuple<T...>>: std::true_type {};

// Compile-time tuple size calculation (non-recursive):
template <typename... Ts>
constexpr std::size_t sizeof_tuple(const std::tuple<Ts...>&) {
  return (sizeof(Ts) + ...);
}

//// Element and target specializations ////
// Serialize unique (non-padded) object to output stream (Binary):
// std::enable_if<std::is_convertable<Target, std::basic_ostream>::value, Target>::type& out
template <typename Target, typename T>
typename std::enable_if<std::has_unique_object_representations_v<T>, void>::type
serialize(Target& out, const T& x) {
  out.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

// Serialize element to std::string:
template <typename T>
void serialize(std::string& s, const T& x) {
  s.append(reinterpret_cast<const char*>(&x), sizeof(x));
}

// Serialize pairs - gets pair elements:
template <typename Target, typename T1, typename T2>
void serialize(Target& out, const std::pair<T1, T2>& p) {
  serialize(out, p.first);
  serialize(out, p.second);
}


//// Tuple unpacking ////
// Forward declaration of top-level tuple template specialization
// - Required to enable recursive tuple unpacking.
template <typename Target, typename... Ts>
void serialize(Target&, const std::tuple<Ts...>&);

// Serialize tuple helper - gets tuple element from index_sequence:
template <typename Target, typename Tuple, size_t... Is>
void serialize_tuple(Target& out, const Tuple& t, std::index_sequence<Is...>) {
  (serialize(out, std::get<Is>(t)), ...);
}

// Serialize tuples - generates index sequence:
template <typename Target, typename... Ts>
void serialize(Target& out, const std::tuple<Ts...>& t) {
  serialize_tuple(out, t, std::index_sequence_for<Ts...>{});
}

template<class Ch, class Tr, class... Ts>
auto& operator<<(std::basic_ostream<Ch, Tr>& os, const std::tuple<Ts...>& t) {
    serialize_tuple(os, t, std::index_sequence_for<Ts...>{});
    return os;
}


//// Generic for_each element in tuple ////
template <std::size_t... Idx>
auto make_index_dispatcher(std::index_sequence<Idx...>) {
    return [] (auto&& f) { (f(std::integral_constant<std::size_t,Idx>{}), ...); };
}

template <std::size_t N>
auto make_index_dispatcher() {
    return make_index_dispatcher(std::make_index_sequence<N>{});
}

template <typename Tuple, typename Func>
void for_each(Tuple&& t, Func&& f) {
    constexpr auto n = std::tuple_size<std::decay_t<Tuple>>::value;
    auto dispatcher = make_index_dispatcher<n>();
    dispatcher([&f,&t](auto idx) { f(std::get<idx>(std::forward<Tuple>(t))); });
}

// example for_each lambdas...
//      auto serialize = [&](auto&& x) {
//        flowStats.write(reinterpret_cast<const char*>(&x), sizeof(x));
//      };
//      std::apply([&](auto& ...x){(..., serialize(x));}, fkt);
//      for_each(fkt, [&](auto&& e) {serialize(flowStats, e);});



class EvalContext;  // forward declaration

class FlowRecord {
public:
//  FlowRecord(u64 flowID, const EvalContext& e) :
//    flowID_(flowID), start_(e.packet().timestamp()),
//    arrival_ns_({0}), bytes_({e.origBytes}),
//    saw_open_(false), saw_close_(false), saw_reset_(false),
//    fragments_(0), retransmits_(0), directionality_(0) {
//  }

  FlowRecord(u64 flowID, const timespec& ts) :
    flowID_(flowID), start_(ts),
    arrival_ns_{}, bytes_{}, protoFlags_{},
    saw_open_(false), saw_close_(false), saw_reset_(false),
    fragments_(0), retransmits_(0), directionality_(0),
    ACK_count_(0), PSH_count_(0), URG_count_(0) {
  }

  u64 update(const EvalContext& e);
  u64 update(const u16 bytes, const timespec& ts);

  u64 getFlowID() const { return flowID_; }
  size_t packets() const { return arrival_ns_.size(); }
  u64 bytes() const;
  const std::string& getLog() const { return log_; }

  timespec last() const {
    constexpr auto NS_IN_SEC = 1000000000LL;

    // TODO: replace this with chrono duration...
    timespec t = start_;
    auto dif = arrival_ns_.back();
    t.tv_nsec += dif % NS_IN_SEC;
    t.tv_sec += dif / NS_IN_SEC;
    if (t.tv_nsec >= NS_IN_SEC) {
      t.tv_sec += t.tv_nsec / NS_IN_SEC;
      t.tv_nsec %= NS_IN_SEC;
    }
    return t;
  }

  const std::vector<u64>& getArrivalV() const { return arrival_ns_; }
  const std::vector<u16>& getByteV() const { return bytes_; }

  // generic flow questions:
  bool isAlive() const { return !(saw_close_ || saw_reset_); }
  bool isTCP() const { return (protoFlags_ & ProtoFlags::isTCP) == ProtoFlags::isTCP; }
  bool isUDP() const { return (protoFlags_ & ProtoFlags::isUDP) == ProtoFlags::isUDP; }
  bool sawSYN() const { return saw_open_; }
  bool sawFIN() const { return saw_close_; }
  bool sawRST() const { return saw_reset_; }

private:
  // Flow Identification:
  u64 flowID_;
  ProtoFlags protoFlags_;
  // also useful to have IPFlags?

  // Time-series:
  timespec start_;
  std::vector<u64> arrival_ns_;
  std::vector<u16> bytes_;

  // Flow State:
  // - Frame Characteristics:
  // e.g. Max frame seen, fragment count, retransmit detection via checksum?
  //      directionality ratio (bytes forward vs. reverse)
  u64 fragments_;
  u64 retransmits_;
  s64 directionality_; // counter: pure ACKs (dec), non-zero payload (inc)

  // - Session Characteristics:
  // e.g. directional initator and directional terminator/finisher?
  //      retransmits
  bool saw_open_;
  bool saw_close_;
  bool saw_reset_;
  // - TCP Specific Characteristics:
  u64 ACK_count_;
  u64 PSH_count_;
  u64 URG_count_;

  // Flow Processing Log:
  std::string log_;
};


class EvalContext {
public:
  util_view::View v;
  u16 origBytes;
  Fields fields;
#ifdef DEBUG_LOG
  std::stringstream extractLog;
#endif

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


#endif
