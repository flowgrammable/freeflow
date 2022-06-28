#include "util_features.hpp"

#include "util_extract.hpp"

#include <random>

static uint16_t randomVariable();


Features::Features(const std::shared_ptr<const Fields> k,
                   const std::shared_ptr<const FlowRecord> r,
                   const std::shared_ptr<const BurstStats> h) :
  k_{k}, r_{r}, h_{h} {
  // blessed initialized to false
}


// Copy Construction (clone with blessed reset)
Features::Features(const Features& f) : k_{f.k_}, r_{f.r_}, h_{f.h_} {
  // blessed initialized to false
//  if (f.blessed)
//    std::cerr << "Features: Blessed resets on copy construction." << std::endl;
}


// Copy Assignment (blindly replaces tracked state)
Features& Features::operator=(const Features& f) {
  k_ = f.k_;
  r_ = f.r_;
  h_ = f.h_;
  blessed = false;  // ensure blessed is false
  return *this;
}


// Copy Assignment (merges tracked state)
// - Throws if tracked objects aren't considered equal.
// - Assumes argument is merged into calling object.
Features& Features::merge(const Features& f) {
  if (r_ != f.r_) {
    throw std::logic_error("Features copy assignment: " \
                           "mismatched update in parent FlowRecord object");
  }
  // Replace FlowRecord..?
  //  r_ = f.r_;

  // Update Fields if defined (per packet):
  if (f.k_) {
    k_ = f.k_;
  }

  // Repalce HitStats only if defined:
  if (f.h_) {
    h_ = f.h_;
    std::cerr << "Suspicious: HitStats shoudn't change for lifetime of Stack_entry." << std::endl;
  }

  // keep blessed untouched.
  return *this;
}


/// Feature gather implementations:
// control feature; not used in actual inference
template<> auto Features::gather_idx(std::integer_sequence<size_t, 0>) const {
  FeatureType x = randomVariable();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 0>) {
  return "Random";
}

/// Stateless Packet Features:
// Mix ip proto with meaningful port:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 1>) const {
  // min(src, dst): smallest port number tends to provide meaning
  FeatureType x = (uint16_t(k_->ipProto)<<8) ^ std::min(k_->srcPort, k_->dstPort);
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 1>) {
  return "Proto Port";
}

// TODO: compare performance of above with better notion of service.
// TODO: compare performance of above with lower 16 bits of ip...
// Directional service by dst:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 2>) const {
  FeatureType x = (k_->ipv4Dst>>16) ^ k_->dstPort;
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 2>) {
  return "Dest Service";
}
// Directional service by src:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 3>) const {
  FeatureType x = (k_->ipv4Src>>16) ^ k_->srcPort;
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 3>) {
  return "Source Service";
}

// Useful flags from IP and TCP headers:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 4>) const {
  FeatureType x = make_flags_bitset(*k_);
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 4>) {
  return "Flags";
}

// Bi-directonal Port:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 5>) const {
  FeatureType x = k_->srcPort ^ k_->dstPort;
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 5>) {
  return "Ports";
}

// Mix ipv4 protocol (8b), tcp flags (9b), and lowest port number:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 6>) const {
  FeatureType x = uint16_t(k_->ipProto) ^ (uint16_t(k_->fTCP)<<7) ^
      std::min(k_->srcPort, k_->dstPort);
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 6>) {
  return "Flags Service";
}

// Host Pair Subnet Association:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 7>) const {
  FeatureType x = (k_->ipv4Dst>>16) ^ (k_->ipv4Src>>16);
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 7>) {
  return "IpPair Upper";
}
template<> auto Features::gather_idx(std::integer_sequence<size_t, 8>) const {
  FeatureType x = (k_->ipv4Dst>>8) ^ (k_->ipv4Src>>8);
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 8>) {
  return "IpPair Mid";
}
template<> auto Features::gather_idx(std::integer_sequence<size_t, 9>) const {
  FeatureType x = (k_->ipv4Dst) ^ (k_->ipv4Src);
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 9>) {
  return "IpPair Low";
}

/// Stateful Flow Features:
// Packets since start of flow as tracked in Flow State:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 10>) const {
  FeatureType x = std::min(r_->packets(), size_t(std::numeric_limits<uint16_t>::max()));
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 10>) {
  return "Packets";
}

template<> auto Features::gather_idx(std::integer_sequence<size_t, 11>) const {
  FeatureType x = k_->ipLength; // combine with frag offset?
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 11>) {
  return "IpLength";
}
template<> auto Features::gather_idx(std::integer_sequence<size_t, 12>) const {
  FeatureType x = k_->ipFlowLabel;
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 12>) {
  return "5Tuple";
}
template<> auto Features::gather_idx(std::integer_sequence<size_t, 13>) const {
  FeatureType x = get<11>() ^ get<12>();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 13>) {
  return "IpLength ^ Tuple";
}

/// Cache Metadata:
// RefCount:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 14>) const {
  FeatureType x = h_ ?
    std::min(std::accumulate(h_->begin(),h_->end(),0), int(std::numeric_limits<uint16_t>::max()))
    : std::numeric_limits<uint16_t>::min();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 14>) {
  return "RefCount";
}
// BurstCount:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 15>) const {
  FeatureType x = h_ ?
    std::min(h_->back(), int(std::numeric_limits<uint16_t>::max()))
    : std::numeric_limits<uint16_t>::min();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 15>) {
  return "BrustCount";
}

// Combine RefCount and BurstCount:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 16>) const {
  FeatureType x = std::min(get<15>(), uint16_t(std::numeric_limits<uint8_t>::max()))<<8 ^
                  get<14>();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 16>) {
  return "Ref ^ Burst";
}
// Combine RefCount and BurstCount and FlowID:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 17>) const {
  FeatureType x = get<16>() ^ get<12>();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 17>) {
  return "Ref ^ Burst ^ Tuple";
}

// Burst and ref count mixed with Host Pair Subnets:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 18>) const {
  FeatureType x = get<16>() ^ get<7>();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 18>) {
  return "Ref ^ Burst ^ Upper";
}
//Burst and ref count mixed with Host Pair Subnets:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 19>) const {
  FeatureType x = get<16>() ^ get<8>();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 19>) {
  return "Ref ^ Burst ^ Mid";
}
// Burst and ref count mixed with Host Pair Subnets:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 20>) const {
  FeatureType x = get<16>() ^ get<9>();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 20>) {
  return "Ref ^ Burst ^ Low";
}

/// Stateful Flow Features:
template<> auto Features::gather_idx(std::integer_sequence<size_t, 21>) const {
  FeatureType x = get<11>() ^ get<7>();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 21>) {
  return "IpLength ^ Upper";
}
template<> auto Features::gather_idx(std::integer_sequence<size_t, 22>) const {
  FeatureType x = get<11>() ^ get<8>();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 22>) {
  return "IpLength ^ Mid";
}
template<> auto Features::gather_idx(std::integer_sequence<size_t, 23>) const {
  FeatureType x = get<11>() ^ get<9>();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 23>) {
  return "IpLength ^ Low";
}
template<> auto Features::gather_idx(std::integer_sequence<size_t, 24>) const {
  FeatureType x = k_->ipFragOffset ^ get<4>() ^ get<13>();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 24>) {
  return "IpLength ^ Tuple ^ Frag";
}

template<> auto Features::gather_idx(std::integer_sequence<size_t, 25>) const {
  FeatureType x = get<4>() ^ get<7>();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 25>) {
  return "Flags ^ Upper";
}

template<> auto Features::gather_idx(std::integer_sequence<size_t, 26>) const {
  FeatureType x = std::numeric_limits<uint16_t>::min();   // always 0
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 26>) {
  return "NULL";
}

template<> auto Features::gather_idx(std::integer_sequence<size_t, 27>) const {
  FeatureType x = std::min(get<15>(), uint16_t(std::numeric_limits<uint8_t>::max()))<<8 ^
                  get<11>();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 27>) {
  return "Burst ^ IpLength";
}
template<> auto Features::gather_idx(std::integer_sequence<size_t, 28>) const {
  FeatureType x = std::min(get<16>(), uint16_t(std::numeric_limits<uint8_t>::max()))<<8 ^
                  get<11>();
  return x;
}
template<> auto Features::name_idx(std::integer_sequence<size_t, 28>) {
  return "Ref ^ Burst ^ IpLength";
}


Features::FeatureVector Features::gather(bool force) const {
  if (!k_ || !r_)
    throw std::logic_error("Gather before init of Feature class.");
  if (!force && !blessed)
    throw std::logic_error("Gather before blessing of Feature class.");

#ifdef SCRIPT_FOLD
  return gather_seq_fold<SCRIPT_FOLD>(FeatureSeq{});
#else
  return gather_seq(FeatureSeq{});
#endif
}


std::array<std::string_view, Features::FEATURES> Features::names() {
  return names_seq(FeatureSeq{});
}


void Features::setBurstStats(std::shared_ptr<const BurstStats> h) {
  h_ = h;
}


void Features::bless() {
  blessed = true;
}


uint16_t randomVariable() {
  static std::random_device rd;     // to generate seed
  static std::default_random_engine e1(rd()); // psudo random number generator
  static std::uniform_int_distribution<uint16_t> uniform_dist;
  return uniform_dist(e1);
}
