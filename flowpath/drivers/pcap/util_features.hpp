#ifndef FEATURES_HPP
#define FEATURES_HPP

#include <memory>
#include <array>
#include <vector>
#include <sstream>
#include <string_view>

//#define SCRIPT_SEQ

// Foward declares from util_extract.hpp
class Fields;
class FlowRecord;
using BurstStats = std::vector<int>;  // mru burst hit counter

template<typename T, T... seq>
std::string print_sequence(std::integer_sequence<T, seq...> int_seq);


// Abstracts gathering of packet / simulation features from consumer.
// Delays gathering until data is needed, requires use of shared pointers.
struct Features {
//  using FeatureSeq = std::make_index_sequence<25>;  // first N (all) features
//  using FeatureSeq = std::make_index_sequence<29>;  // first N (all) features
//  using FeatureSeq = std::integer_sequence<size_t, 0, 6,15,11,1,21,4>;
#ifdef SCRIPT_SEQ
  using FeatureSeq = std::integer_sequence<size_t, 0, SCRIPT_REPLACE>;
#else
  using FeatureSeq = std::integer_sequence<size_t, 0,
  6,21,25,18,10,22,19,27,8,20>;
#endif
//6,21,25,18,10,22,19,27,8,20,28,11,4,7,17,14,3,2,15,23,1,16,9,13,5,26,24,12>
//6,21,25,18,10,22,19,27,8,20>;
//6,21,25,18,10,22,19,27,8,20,4,7,17,14,3,2,15,23,1,16,9,13,5,26,11,24,12,28>;
//6,4,28,27,11,21,25,22,10,7,8,18,1,3,2,19,23,9,5,20,13,12,17,24,26,0,14,15,16>;
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
  // Gather values for selected features:
  template<size_t... seq> FeatureVector gather_seq() const;
  template<size_t... seq> FeatureVector gather_seq(std::integer_sequence<size_t, seq...>) const;
  template<size_t Index> auto gather_idx(std::integer_sequence<size_t, Index>) const;
  template<size_t Index> FeatureType get() const;  // helper mimicing std::get

  // Gather names for selected features:
  template<size_t... seq> constexpr static auto names_seq();
  template<size_t... seq> constexpr static auto names_seq(std::integer_sequence<size_t, seq...>);
  template<size_t Index> constexpr static auto name_idx(std::integer_sequence<size_t, Index>);

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
    gather_idx(std::integer_sequence<size_t, seq>())... };
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
  return gather_idx(std::integer_sequence<size_t, Index>());
}

template<size_t... seq>
constexpr auto Features::names_seq() {
  constexpr size_t FEATURES = sizeof...(seq);
  std::array<std::string_view, FEATURES> names = {
    name_idx(std::integer_sequence<size_t, seq>())... };
  return names;
}

template<size_t... seq>
constexpr auto Features::names_seq(std::integer_sequence<size_t, seq...>) {
  return names_seq<seq...>();
}

template<typename T, T... seq>
std::string print_sequence(std::integer_sequence<T, seq...> int_seq) {
  std::stringstream ss;
  ss << "Features<" << int_seq.size() << ">: ";
  ((ss << seq << ' '), ...);
  return ss.str();
}

template<typename T, T... seq>
std::array<T, sizeof...(seq)> sequence2array(std::integer_sequence<T, seq...> int_seq) {
  std::array<T, sizeof...(seq)> a = { seq... };
  return a;
}


#endif // FEATURES_HPP
