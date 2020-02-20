#include "util_features.hpp"

#include "util_extract.hpp"


Features::Features(const std::shared_ptr<const Fields> k,
                   const std::shared_ptr<const FlowRecord> r) : k_(k), r_(r) {}


Features::FeatureType Features::gather() const {
  FeatureType f;
//  if (!k_ || !r_) {
//    return f; // returns jibberish
//  }

  // Stateless Packet Features:
  // Mix ip proto to upper 8 bits of meaningful port:
  // min(src, dst): smallest port number tends to provide meaning
  f[0] = (uint16_t(k_->ipProto) << 8) ^ std::min(k_->srcPort, k_->dstPort);
  // Mix dst_ip/16 and dest_port:
  f[1] = (k_->ipv4Dst >> 16) ^ k_->dstPort;
  f[2] = (k_->ipv4Src >> 16) ^ k_->srcPort;
  // Useful flags from IP and TCP headers:
  f[3] = make_flags_bitset(*k_);
  f[4] = k_->srcPort ^ k_->dstPort;
  f[5] = uint16_t(k_->ipProto) ^ (uint16_t(k_->fTCP) << 7) ^ std::min(k_->srcPort, k_->dstPort);
  f[6] = (k_->ipv4Dst >> 16) ^ (k_->ipv4Src >> 16);
  f[7] = (k_->ipv4Dst >> 8) ^ (k_->ipv4Src >> 8);
  f[8] = (k_->ipv4Dst) ^ (k_->ipv4Src);

  // Stateful Flow Features:
  f[9] = r_->tcp_state(); // 4 bits
  f[10] = std::min(r_->packets(), size_t(std::numeric_limits<uint16_t>::max()));
  return f;
}
