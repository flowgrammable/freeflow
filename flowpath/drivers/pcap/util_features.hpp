#ifndef FEATURES_HPP
#define FEATURES_HPP

#include <memory>
#include <array>
//#include <vector>

// Foward declares from util_extract.hpp
class Fields;
class FlowRecord;

// Abstracts gathering of packet / simulation features from consumer.
// Delays gathering until data is needed, requires use of shared pointers.
class Features {
public:
  using FeatureType = std::array<uint16_t, 12>;
//  static constexpr size_t TABLES = 11;
  Features() = default;
  Features(std::shared_ptr<const Fields> k,
           std::shared_ptr<const FlowRecord> r);

//  Features(Features&&) = default;
//  Features(const Features&) = default;
//  Features& operator=(Features&&) = default;

//  // Returns organization of perceptron tables:
//  std::vector<size_t> tables() const {
//    // all tables < 64k in size
//    return std::vector<size_t>(TABLES, 64*1024);
//  }

  // Gathers components into features:
  FeatureType gather() const;

  // Arguments
  private:
  std::shared_ptr<const Fields> k_;
  std::shared_ptr<const FlowRecord> r_;
};

#endif // FEATURES_HPP
