#ifndef FEATURES_HPP
#define FEATURES_HPP

#include <memory>
#include <array>
#include <vector>
#include <string_view>

// Foward declares from util_extract.hpp
class Fields;
class FlowRecord;
using BurstStats = std::vector<int>;  // mru burst hit counter


// Abstracts gathering of packet / simulation features from consumer.
// Delays gathering until data is needed, requires use of shared pointers.
struct Features {
  using FeatureSeq = std::make_index_sequence<25>;  // first N (all) features
//  using FeatureSeq = std::integer_sequence<size_t, 0,1,2,3>;
  constexpr static size_t FEATURES = FeatureSeq{}.size();
  using FeatureType = uint16_t;
  using FeatureVector = std::array<FeatureType, FEATURES>;

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

  FeatureVector gather(bool force = false) const;
  static std::array<std::string_view, FEATURES> names();

private:
  // Gather values for features sequence:
  template<size_t... seq> FeatureVector gather_seq() const;
  template<size_t... seq> FeatureVector gather_seq(std::integer_sequence<size_t, seq...>) const;
  template<size_t Index> auto gather_idx(std::integer_sequence<size_t, Index>) const;
  template<size_t Index> FeatureType get() const;  // helper mimicing std::get

  // Gather names for feature sequence;
  template<size_t... seq> auto names_seq() const;
  template<size_t... seq> auto names_seq(std::integer_sequence<size_t, seq...>) const;
//  auto names_cheat() const;

  // Handles to original structures:
private:
  std::shared_ptr<const Fields> k_;
  std::shared_ptr<const FlowRecord> r_;
  std::shared_ptr<const BurstStats> h_;
  bool blessed = false;   // blessed as initialized by simulation
};


// Initializes feature vector from template parameter pack:
template<size_t... seq>
Features::FeatureVector Features::gather_seq() const {
  constexpr size_t FEATURES = sizeof...(seq);
  FeatureVector features = {
    std::get<0>(gather_idx(std::integer_sequence<size_t, seq>()))... };
  return features;
}

// Initializes feature vector from integer_sequence argument:
template<size_t... seq>
Features::FeatureVector Features::gather_seq(std::integer_sequence<size_t, seq...>) const {
  return gather_seq<seq...>();
}

// Helper to generate call to gather_index(N) from get<N>; mimiking std::get<N>
template<size_t Index>
Features::FeatureType Features::get() const {
  return std::get<0>(gather_idx(std::integer_sequence<size_t, Index>()));
}

template<size_t... seq>
auto Features::names_seq() const {
  constexpr size_t FEATURES = sizeof...(seq);
  std::array<std::string_view, FEATURES> names = {
    std::get<1>(gather_idx(std::integer_sequence<size_t, seq>()))... };
  return names;
}

template<size_t... seq>
auto Features::names_seq(std::integer_sequence<size_t, seq...>) const {
  return names_seq<seq...>();
}


#endif // FEATURES_HPP
