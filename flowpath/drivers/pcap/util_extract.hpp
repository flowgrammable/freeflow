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

using IpTuple = std::tuple<u32, u32>;
using PortTuple = std::tuple<u16, u16>;
using FlowKeyTuple = std::tuple<IpTuple, PortTuple, u8>;

std::string make_flow_key_string(const Fields& k);
FlowKeyTuple make_flow_key_tuple(const Fields& k);


// Serialize element:
template <typename T>
void serialize(std::ostream& os, const T& x) {
  os.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

// Serialize pairs - gets pair elements:
template <typename T1, typename T2>
void serialize(std::ostream& os, const std::pair<T1, T2>& p) {
  serialize(os, p.first);
  serialize(os, p.second);
}

// Serialize tuple helper - gets tuple element from index_sequence:
template <typename Tuple, size_t... Is>
void serialize_tuple(std::ostream& os, const Tuple& t, std::index_sequence<Is...>) {
  (serialize(os, std::get<Is>(t)), ...);
}

// Serialize tuples - generates index sequence:
template <typename... Ts>
void serialize(std::ostream& os, const std::tuple<Ts...>& t) {
  serialize_tuple(os, t, std::index_sequence_for<Ts...>{});
}


//template<class Ch, class Tr, class Tuple, std::size_t... Is>
//void serialize_tuple_impl(std::basic_ostream<Ch,Tr>& os,
//                      const Tuple& t,
//                      std::index_sequence<Is...>)
//{
////    ((os << (Is == 0? "" : ", ") << std::get<Is>(t)), ...);
////  ((os << std::get<Is>(t)), ...);
//  ((os << std::get<Is>(t)), ...);
////  (os.write(os << std::get<Is>(t)), ...);
////  ((std::cout << std::get<Is>(t) << std::endl), ...);
//}

//template<class Ch, class Tr, class... Args>
//auto& operator<<(std::basic_ostream<Ch, Tr>& os,
//                 const std::tuple<Args...>& t)
//{
//    serialize_tuple_impl(os, t, std::index_sequence_for<Args...>{});
//    return os;
//}


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

private:
  // Flow Identification:
  u64 flowID_;
  std::string flowKey_;
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
