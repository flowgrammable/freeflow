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
  if (f.blessed)
    std::cerr << "Blessed Features reset to on copy construction." << std::endl;
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


Features::FeatureType Features::gather(bool force) const {
  FeatureType f;
  if (!k_ || !r_ || !(force || h_))
    throw std::logic_error("Gather before init of Feature class.");
  if (!force && !blessed)
    throw std::logic_error("Gather before blessed of Feature class.");

  // Random variable used for bias analysis (not actually used in inference):
  f[0] = randomVariable();  // control; not used in actual decision

  /// Stateless Packet Features:
  // Mix ip proto to upper 8 bits of meaningful port:
  // min(src, dst): smallest port number tends to provide meaning
  f[1] = (uint16_t(k_->ipProto) << 8) ^ std::min(k_->srcPort, k_->dstPort);
  // Mix dst_ip/16 and dest_port:

  // Directional service association:
  f[2] = (k_->ipv4Dst >> 16) ^ k_->dstPort;         // (#8, noisy learning)
  f[3] = (k_->ipv4Src >> 16) ^ k_->srcPort;         // (#3, slow learning)
  // TODO: compare performance of above with better notion of service.
  // TODO: compare performance of above with lower 16 bits of ip...

  // Useful flags from IP and TCP headers:
  f[4] = make_flags_bitset(*k_);                    // (#7, fast learning, flat)
  f[5] = k_->srcPort ^ k_->dstPort;                 // (#9, flat learning)

  // Mixing of ipv4 protocol (8b), tcp flags (9b), and lowest port number:
  f[6] = uint16_t(k_->ipProto) ^ (uint16_t(k_->fTCP) << 7) ^ std::min(k_->srcPort, k_->dstPort);  // (#6, adaptive)

  // Host Pair Subnet Association:
  f[7] = (k_->ipv4Dst >> 16) ^ (k_->ipv4Src >> 16); // (#4, learning)
  f[8] = (k_->ipv4Dst >> 8) ^ (k_->ipv4Src >> 8);   // (#5, learning)
  f[9] = (k_->ipv4Dst) ^ (k_->ipv4Src);             // (#6, learning)


  /// Stateful Flow Features:
  f[10] = r_->tcp_state(); // 4 bits (#10)
  // Packets since start of flow as tracked in Flow State:
  f[11] = std::min(r_->packets(), size_t(std::numeric_limits<uint16_t>::max()));  // (#1, degrading)

  // Cache Metadata:
  if (h_) {
    f[12] = std::min(std::accumulate(h_->begin(), h_->end(), 0), int(std::numeric_limits<uint16_t>::max()));  // ref count
    f[13] = std::min(h_->back(), int(std::numeric_limits<uint16_t>::max()));  // burst count
  }
  else {
//    std::cerr << "HitStats unavailable at time of Features.gather()" << std::endl;
    f[12] = 0;
    f[13] = 0;
  }

  return f;
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
