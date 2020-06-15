#ifndef FEATURES_HPP
#define FEATURES_HPP

#include <memory>
#include <array>
#include <vector>

// Foward declares from util_extract.hpp
class Fields;
class FlowRecord;
using BurstStats = std::vector<int>;  // mru burst hit counter


// Abstracts gathering of packet / simulation features from consumer.
// Delays gathering until data is needed, requires use of shared pointers.
struct Features {
  using FeatureType = std::array<uint16_t, 14>;
//  static constexpr size_t TABLES = 11;
  Features() = default;
  Features(std::shared_ptr<const Fields> k,
           std::shared_ptr<const FlowRecord> r,
           std::shared_ptr<const BurstStats> h = {});

  // Move (preserves blessed state)
  Features(Features&&) = default;
  Features& operator=(Features&&) = default;

  // Copy (resets blessed state)
  Features(const Features&);
  Features& operator=(const Features&);

//  // Returns organization of perceptron tables:
//  std::vector<size_t> tables() const {
//    // all tables < 64k in size
//    return std::vector<size_t>(TABLES, 64*1024);
//  }

  // Simulator Interface:
  void setBurstStats(std::shared_ptr<const BurstStats>);
  Features& merge(const Features& f); // copy assignment that merges state
  void bless();
  FeatureType gather(bool force = false) const;

  // Handles to original structures:
private:
  std::shared_ptr<const Fields> k_;
  std::shared_ptr<const FlowRecord> r_;
  std::shared_ptr<const BurstStats> h_;
  bool blessed = false;   // blessed as initialized by simulation
};

#endif // FEATURES_HPP
