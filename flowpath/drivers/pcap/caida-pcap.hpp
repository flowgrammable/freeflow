#ifndef DRIVER_CAIDA_PCAP_HPP
#define DRIVER_CAIDA_PCAP_HPP

#include "util_bitmask.hpp"

#include "system.hpp"
//#include "dataplane.hpp"
//#include "application.hpp"
//#include "port_table.hpp"
//#include "port.hpp"
#include "port_pcap.hpp"
#include "context.hpp"

#include "util_view.hpp"


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

//using IpTuple = std::tuple<u32, u32>;   // {src, dst}
//using PortTuple = std::tuple<u16, u16>; // {src, dst}
//using ProtoTuple = std::tuple<u8>;
//using FlowKeyTuple = std::tuple<IpTuple, PortTuple, ProtoTuple>;

//namespace std {
//  template <>
//  class hash<FlowKeyTuple> {
//    using result_type = std::size_t;
//    using object_type = FlowKeyTuple;

//    result_type operator()(const object_type& x) const {
//      IpTuple ip = std::get<0>(x);
//      result_type h1 = std::get<0>(ip) << ;
////      std::tie() = x;

////      result_type const h1 = std::tie()
//    }
//  }
//}

std::string make_flow_key(const Fields& k) {
//  const FlowKeyTuple fkt {
//    IpTuple{k.ipv4Src, k.ipv4Dst},
//    PortTuple{k.srcPort, k.dstPort},
//    ProtoTuple{k.ipProto}
//  };
//  std::string fks(reinterpret_cast<const char*>(&fkt), sizeof(fkt));
//  assert(fks.size() == sizeof(fkt));

//  struct __attribute__((packed)) {
//    IpTuple ip{k.ipv4Src, k.ipv4Dst};
//    PortTuple port{k.srcPort, k.dstPort};
//    ProtoTuple proto{k.ipProto};
//  } fkt;

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
//  const bool activeSince(const timespec& t) const {
//    return flowR.saw_close_ || flowR.saw_reset_
//  }

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
  util_view::View v;
  u16 origBytes;
  Fields fields;
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
