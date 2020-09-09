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
#include "types.hpp"
#include "vector"
#include "bitset"

#include "util_view.hpp"

#include <ostream>

//#include "absl/utility/utility.h"
//#include <utility>
//#include <type_traits>


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


// Definition of useful Flags/Metadata
// - Union of all possible flags our huristic cares about...
// - Flags are not limited to header bits, but can also be metadata...
enum class ProtoFlags {
  // add DNS, ICMP, etc?
  isUDP =1<<3,
  isTCP =1<<2,
  isIPv6=1<<1,
  isIPv4=1<<0
  // Add TCP flow-state?
  // - e.g. window size, keep-alive prediction?
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
  TCPFlags fTCP;

  // Ethernet Fields:
  u64 ethSrc = 0;
  u64 ethDst = 0;
  u16 ethType = 0;
  u16 vlanID = 0;

  // IP Fields:
  u32 ipv4Src = 0;  // not ipv6 compatible
  u32 ipv4Dst = 0;  // not ipv6 compatible
  u16 ipLength = 0; // ipv4: total length; ipv6: payload length (no option support)
  u16 ipFragOffset = 0; // not aware of ipv6
  u8  ipProto = 0;  // not aware of ipv6 options
  u8  ipTC = 0;     // {6'b DSCP, 2'b ECN}
  u16 ipFlowLabel = 0;  // ipv4: conventional 5-tuple; ipv6: {20'b FL}
  IPFlags fIP;

  // Transport Layer (TCP/UDP)
  u16 srcPort = 0;
  u16 dstPort = 0;

  // TCP Specific Fields:
  u32 tcpSeqNum = 0;
  u32 tcpAckNum = 0;
  u16 tcpWindow = 0; // multiply by window scaling factor for real bytes.
  // TODO: delta in sequence number?
  // Add Data Offset?
  u8 tcpOffset = 0;
};

std::string print_ip(uint32_t);
std::string print_flow_key_string(const Fields&);
std::string print_flow_key_string(const FlowKeyTuple&);
std::string make_flow_key_string(const Fields&);
FlowKeyTuple make_flow_key_tuple(const Fields&);
Flags make_flags_bitset(const Fields&);

// Sanity checks:
static_assert(sizeof(Flags) == 2, "Flags is not 2B?");
static_assert(!std::is_integral<FlowKeyTuple>::value, "FKT is integral?!");
//static_assert(std::has_unique_object_representations_v<Ipv4Tuple>, "Ipv4Tuple has padding!");
static_assert(std::has_unique_object_representations_v<TCPFlags>, "TCPFlags has padding!");

class EvalContext;  // forward declaration

class FlowRecord {
public:
  enum MODE_EN {NORMAL, TIMESERIES};

  FlowRecord(u64 flowID, const timespec& ts, MODE_EN m = NORMAL) :
    MODE_(m), flowID_(flowID), flowTuple_{}, start_(ts) {}

  FlowRecord(u64 flowID, const FlowKeyTuple& flowTuple, const timespec& ts, MODE_EN m = NORMAL) :
    MODE_(m), flowID_(flowID), flowTuple_(flowTuple), start_(ts) {}

//  FlowRecord(FlowRecord&&) = default;
//  FlowRecord(const FlowRecord&) = default;
//  FlowRecord& operator=(const FlowRecord&) = default;

  u64 update(const EvalContext& e);
  u64 update(const u16 bytes, const timespec& ts);

  u64 getFlowID() const { return flowID_; }
  FlowKeyTuple getFlowTuple() const { return flowTuple_; }
  size_t packets() const { return pkts_; }
  u64 bytes() const {return bytes_; }

  std::pair<u64, timespec> last() const {
    constexpr auto NS_IN_SEC = 1000000000LL;

    // TODO: replace this with chrono duration...
    timespec t = start_;
    u64 dif = arrival_ns_ts_.back();
    t.tv_nsec += dif % NS_IN_SEC;
    t.tv_sec += dif / NS_IN_SEC;
    if (t.tv_nsec >= NS_IN_SEC) {
      t.tv_sec += t.tv_nsec / NS_IN_SEC;
      t.tv_nsec %= NS_IN_SEC;
    }
    return std::make_pair(dif, t);
  }

  const std::vector<u64>& getArrivalV() const { return arrival_ns_ts_; }
  const std::vector<u16>& getByteV() const { return byte_ts_; }

  // Generic flow questions:
  bool isTCP() const { return (protoFlags_ & ProtoFlags::isTCP) == ProtoFlags::isTCP; }
  bool isUDP() const { return (protoFlags_ & ProtoFlags::isUDP) == ProtoFlags::isUDP; }

  // TCP flow questions:
  bool sawSYN() const { return saw_open_; }
  bool sawFIN() const { return saw_close_; }
  bool sawRST() const { return saw_reset_; }
  bool isAlive() const { return !(saw_close_ || saw_reset_); }
  u32 lastSeq() const { return last_seq_; }

  // TCP flow features:
//  auto tcp_state() const {return std::vector{isTCP(), saw_open_, saw_close_, saw_reset_};}
  auto tcp_state() const {
    std::bitset<4> bits;
    bits.set(0, isTCP());
    bits.set(1, saw_open_);
    bits.set(2, saw_close_);
    bits.set(3, saw_reset_);
    return uint8_t(bits.to_ulong());
  }

private:
  // Flow Identification:
  const u64 flowID_;
  const FlowKeyTuple flowTuple_;
  ProtoFlags protoFlags_ = {};
  // also useful to have IPFlags?

  // Time-series:
  const MODE_EN MODE_;
  timespec start_;
  u64 pkts_ = 0;
  u64 bytes_ = 0;
  std::vector<u64> arrival_ns_ts_;
  std::vector<u16> byte_ts_;

  // Flow State:
  // - Frame Characteristics:
  // e.g. Max frame seen, fragment count, retransmit detection via checksum?
  //      directionality ratio (bytes forward vs. reverse)
  u64 fragments_ = 0;
  u64 retransmits_ = 0;
  s64 directionality_; // counter: pure ACKs (dec), non-zero payload (inc)
  // - TCP Specific Characteristics:
  u32 syn_seq_ = 0;
  u32 fin_seq_ = 0;
  u32 rst_seq_ = 0;

  u32 first_seq_ = 0;
  u32 last_seq_ = 0;
  u32 first_ack = 0;
  u32 last_ack = 0;
  u32 seq_rollover = 0;
  u32 ack_rollover = 0;

  // - Session Characteristics:
  // e.g. directional initator and directional terminator/finisher?
  //      retransmits
  // todo: repalce bool with sequence numbers...
  bool saw_open_ = false;
  bool saw_close_ = false;
  bool saw_reset_ = false;
  // - TCP Specific Characteristics:
  u64 ACK_count_ = 0;
  u64 PSH_count_ = 0;
  u64 URG_count_ = 0;

  // Flow Processing Log:
//  std::ostream& log_;
};


class EvalContext {
public:
  util_view::View v;
  u16 origBytes;
  std::shared_ptr<Fields> fields;
#ifdef DEBUG_LOG
  std::stringstream extractLog;
#endif

  EvalContext(fp::Packet* const p);
  EvalContext(const fp::Packet& p);
  EvalContext(const EvalContext& cxt) = default;
  EvalContext(EvalContext&& cxt) = default;

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

std::ostream& print_eval(std::ostream &out, const EvalContext& evalCxt);
std::stringstream hexdump(const void* msg, size_t bytes);
void print_hex(const std::string& s);
std::stringstream dump_packet(const fp::Packet& p);
void print_packet(const fp::Packet& p);
std::stringstream print_flow(const FlowRecord& flow);
void print_log(const EvalContext& e);
void print_log(const FlowRecord& flow);

#endif
