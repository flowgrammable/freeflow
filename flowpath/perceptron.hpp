#ifndef PERCEPTRON_HPP
#define PERCEPTRON_HPP

#include <cstdint>
#include <vector>
#include <algorithm>
#include <numeric>
#include <sstream>

#include "types.hpp"
#include "drivers/pcap/util_io.hpp"

//#include "global_config.hpp"
//extern struct global_config CONFIG;

namespace entangle {

// Example FeatureGen that produces reduces a generic tuple:
template<size_t index_bits, typename Tuple>
class FeatureGen {
  auto operator()(const Tuple& x) const {
    constexpr uint64_t MIX = 0x9e37'79b9'7f4a'7c16LLU;
    constexpr static size_t Features = std::tuple_size<Tuple>::value;
    //  std::apply([this](auto& ...x) { (..., internal_combine(x)); }, tuple);
    return std::hash<Tuple>{}(x);
  }
};


// Expect Key to be an integral_type.
// - Table size is determined by constructor.
template<typename Key>
class PerceptronTable {
public:
  static constexpr bool ENABLE_STATS = false; // inference debug

  static constexpr size_t BITS_ = 5;  // resolution of single perceptron
  using Perceptron = ClampedInt<BITS_>;

  PerceptronTable(size_t elements);

  auto inference(Key k);
  void train(Key k, bool correlation);

  float dynamic_information(Key k) const;
  std::string print_stats() const;

  auto inference_internal(Key k);   // doesn't contribute to stats

private:
  const size_t MAX_;
  std::vector<Perceptron> table_;   // perceptron table for each feature

public:
  // Utilization Metadata:
  std::vector<int64_t> touch_inference_;  // counts inference events
  std::vector<int64_t> touch_train_;      // counts train events
//  std::vector<int8_t> inference_history_; // sequential history of inferences
};


// Struct for inference statistics:
struct inference_result {
  uint8_t confidence = 0;  // inference_threshold - sum
  int8_t saturation = 0;  // training_threshold - sum
  int8_t agreement = 0;   // # of features above threshold
  int8_t information = 0; // saturating diff of train - inference (>0 indicates lots of training events)
};


// Expect Key to be a std::array<integral_type, N> or std::tuple<integral_type...>
// - Option 1: Key is folded to single table.
// - Option 2: Each feature in key has a dedicated table.
template<class Key>
class HashedPerceptron {
public:
  static constexpr bool ENABLE_STATS = PerceptronTable<Key>::ENABLE_STATS;
  static constexpr bool ENABLE_CORRELATION_ANALYSIS = false;

  using Feature = typename Key::value_type;
  using FeatureTable = PerceptronTable<Feature>;
  constexpr static size_t TABLES_ = std::tuple_size<Key>::value;
  using Weights = std::array<int8_t, TABLES_>;

  HashedPerceptron(std::vector<size_t> elements);
  HashedPerceptron(size_t elements);

  auto inference(Key k);
  void reinforce(Key k, bool positive);   // positive/negative correlation
  void force_updates(bool force = true);

  std::string print_stats();
  void clear_stats();
  auto information_raw(Key tuple) const;
  const std::vector<FeatureTable>& get_tables() const;

private:
  auto inference_raw(Key tuple);
  auto inference_internal(Key tuple);
  void inference_event(Weights w);

  // Member Variables:
  std::vector<FeatureTable> tables_;  // 1 table per feature
  int64_t training_threshold_;
  int64_t inference_threshold_;
  bool force_update_ = false;

public:
  // Training Stats:
  int64_t train_total_ = 0;     // total reinforce calls
  int64_t train_occured_ = 0;   // reinforce calls actually followed through
  std::vector<inference_result> inference_stats_;

private:
  // Feature Stats:
  using CorrelationMatrix = std::array<std::array<int64_t, TABLES_>, TABLES_>;
  CorrelationMatrix feature_correlation_ {{}};
  std::array<int64_t, TABLES_> feature_s1_ {}; // sum, power 1 (for std. deviation)
  int64_t feature_n_ = {};  // number of inferences (for std. deviation)

  // File Output Stats:
  CSV csv_fCorrelation_;  // mult+add (feature x feature)
  CSV csv_fS1_;           // sum (per feature)
  // S2 (feature^2 per feature) is identity in correlation matrix.
  CSV csv_fN_;            // inferences (per feature)
  // TODO: add raw inference output?
};


///////////////////////////////////////////////////////////////////////////////
/// PerceptronTable Implementation:
/// - Low-level table implementation.

template<typename Key>
PerceptronTable<Key>::PerceptronTable(size_t elements) :
  MAX_(elements), table_(elements) {
  if (ENABLE_STATS) {
    touch_inference_.resize(elements);
    touch_train_.resize(elements);
  }
}

template<typename Key>
auto PerceptronTable<Key>::inference_internal(Key k) {
  assert(k < MAX_);
  return table_[k];
}

template<typename Key>
auto PerceptronTable<Key>::inference(Key k) {
  auto ret = inference_internal(k);
  if (ENABLE_STATS) {
    ++touch_inference_[k];
//    inference_history_.push_back(ret.get());
  }
  return ret;
}

template<typename Key>
void PerceptronTable<Key>::train(Key k, bool correlation) {
  assert(k < MAX_);
  if (ENABLE_STATS) {
    ++touch_train_[k];
  }
  correlation ? ++table_[k] : --table_[k];
}

// Returns rough estimate of dynamic information stored in weight:
// - Ratio of inferences over training.
template<typename Key>
float PerceptronTable<Key>::dynamic_information(Key k) const {
  assert(k < MAX_);
  return touch_inference_[k] / (touch_train_[k] + touch_inference_[k]);
}

// Prints distrubtion of training & inference events for all feature weights:
template<typename Key>
std::string PerceptronTable<Key>::print_stats() const {
  // IMPLEMENT ME!
  return std::string();
}


///////////////////////////////////////////////////////////////////////////////
/// HashedPerceptron Implementation:
/// - High-level perceptron logic.

template<class Key>
HashedPerceptron<Key>::HashedPerceptron(std::vector<size_t> elements) {
  assert(TABLES_ == elements.size());

  for (size_t e : elements) {
    tables_.emplace_back(e);
  }
  // set training & inference threshold based on # of tables?
  training_threshold_ = TABLES_ * (FeatureTable::Perceptron::RANGE/2);
  inference_threshold_ = 0;
}

template<class Key>
HashedPerceptron<Key>::HashedPerceptron(size_t elements) :
  tables_(TABLES_, elements) {

  // set training & inference threshold based on # of tables?
//  size_t range = size_t(1) << (FeatureTable::BITS_-1);
  training_threshold_ = TABLES_ * (FeatureTable::Perceptron::RANGE/2);
  inference_threshold_ = 0;

  if (ENABLE_CORRELATION_ANALYSIS) {
    csv_fCorrelation_ = CSV("feature_correlation.csv");
    csv_fS1_ = CSV("feature_s1.csv");
    csv_fN_ = CSV("feature_n.csv");
  }
}

template<class Key>
void HashedPerceptron<Key>::force_updates(bool force) {
  force_update_ = force;
}

template<class Key>
auto HashedPerceptron<Key>::inference_raw(Key tuple) {
  Weights w;
  for (size_t i = 0; i < TABLES_; ++i) {
    w[i] = tables_[i].inference(tuple[i]).get();
  }
  if (ENABLE_CORRELATION_ANALYSIS) {
    inference_event(w);
  }
  return w;
}

// Performs inference without noting stats:
template<class Key>
auto HashedPerceptron<Key>::inference_internal(Key tuple) {
  Weights w;
  for (size_t i = 0; i < TABLES_; ++i) {
    w[i] = tables_[i].inference_internal(tuple[i]).get();
  }
  const int sum = std::accumulate(std::begin(w), std::end(w), 0);
  return std::make_pair(sum >= inference_threshold_, sum);
}

template<class Key>
auto HashedPerceptron<Key>::inference(Key tuple) {
  const Weights w = inference_raw(tuple);
  const int sum = std::accumulate(std::begin(w)+1, std::end(w), 0);
  auto ret = std::make_pair(sum >= inference_threshold_, sum);

  if (ENABLE_STATS) {
    const int magnitude = std::abs(sum);
    uint8_t confidence = (256*(magnitude - inference_threshold_))/(TABLES_*FeatureTable::Perceptron::RANGE);
    int8_t saturation = ClampDown<int8_t>(magnitude - training_threshold_);
    int8_t agreement = 0;
    for (auto score : w) {
      const int scaled_tr = inference_threshold_/std::size(w);
      if ( score >= scaled_tr)
        ++agreement;
      else
        --agreement;
    }
//    auto agreement = std::count_if(std::begin(w), std::end(w),
//      [&](auto score) {  // # of features above scaled inference threshold
//        const auto scaled_tr = inference_threshold_/std::size(w);
//        return score >= scaled_tr;
//    });
    auto information = [&]() {
      // Calculate median of dynamic_information metric:
      auto v = information_raw(tuple);  // FIXME!
      auto median_it = std::begin(v) + std::size(v)/2;
      std::nth_element(std::begin(v), median_it, std::end(v));
      return int8_t(*median_it * 100);  // % cast to 8-bit int.
    }();
    inference_stats_.emplace_back(inference_result{confidence, saturation, agreement, information});
//    std::cout << "inference: " << ret.second << (ret.first ? " (evict) " : " (cache-friendly) ")
//              << int(confidence) << " "
//              << int(saturation) << " "
//              << int(agreement)  << " "
//              << int(information) << std::endl;
  }

  return ret;
}

template<class Key>
void HashedPerceptron<Key>::reinforce(Key tuple, bool correlation) {
  ++train_total_;
  const auto [prediction, sum] = inference_internal(tuple);
  const bool incorrect = prediction != correlation;
  const bool update = force_update_ || incorrect || std::abs(sum) < training_threshold_;
  if (ENABLE_STATS) {
//    std::cout << "train: " << sum << " " << (update ? (incorrect ? "(train)" : "(reinforce)") : "(skip)") << std::endl;
  }
  if (update) {
    ++train_occured_;
    // Perform update:
    for (size_t i = 0; i < TABLES_; ++i) {
      tables_[i].train(tuple[i], correlation);
    }
  }
}

template<class Key>
auto HashedPerceptron<Key>::information_raw(Key tuple) const {
  std::array<float, TABLES_> inf;
  for (size_t i = 0; i < TABLES_; ++i) {
    inf[i] = tables_[i].dynamic_information(tuple[i]);
  }
  return inf;
}

// Prints stats over history of training events:
template<typename Key>
std::string HashedPerceptron<Key>::print_stats() {
  std::stringstream ss;
  auto v = inference_stats_;  // local mutable copy of huge stats vector...

  // Print utility of all weight tables:
  for (const auto& t : tables_) {
    // somehow accumulate / append stats for each table?
  }

  // Print stats over history of inference events:
  for (const auto [confidence, saturation, agreement, information] : inference_stats_) {
    // accumulate stats?
  }

  if (std::size(v) > 0) {
    auto median_it = std::begin(v) + std::size(v)/2;
    std::nth_element(std::begin(v), median_it, std::end(v),
      [](const inference_result& a, const inference_result& b) {
        return a.confidence > b.confidence;
      }
    );
    int median_confidence = int(median_it->confidence);

    std::nth_element(std::begin(v), median_it, std::end(v),
      [](const inference_result& a, const inference_result& b) {
        return a.agreement > b.agreement;
      }
    );
    int median_agreement = int(median_it->agreement);

    std::nth_element(std::begin(v), median_it, std::end(v),
      [](const inference_result& a, const inference_result& b) {
        return a.saturation > b.saturation;
      }
    );
    int median_saturation = int(median_it->saturation);

    std::nth_element(std::begin(v), median_it, std::end(v),
      [](const inference_result& a, const inference_result& b) {
        return a.information > b.information;
      }
    );
    int median_information = int(median_it->information);


    auto predict_positive = std::count_if(std::begin(v), std::end(v),
      [](const inference_result& a) {
        return a.agreement >= 0;
      }
    );
    double inference_bias = double(predict_positive) / v.size();

    // TODO: Fixme!
    ss << v.size() << " inferences ("
       << inference_bias*100 << "% T)\t"
       << "[Confidence (" << median_confidence
       << "), Agreement (" << median_agreement
       << "), Saturation (" << median_saturation
       << "), Information (" << median_information
       << ")]\n";
  }

  if (ENABLE_CORRELATION_ANALYSIS) {
    for (auto r : feature_correlation_) {
      csv_fCorrelation_.print(r);
    }
    feature_correlation_ = {{}};  // zero out
    csv_fS1_.print(feature_s1_);
    feature_s1_ = {};  // zero out
    csv_fN_.print(std::make_tuple(feature_n_));
    feature_n_ = {};  // zero out
  }
  return ss.str();
}

template<class Key>
void HashedPerceptron<Key>::inference_event(Weights w) {
  for (size_t i = 0; i < TABLES_; ++i) {
    for (size_t j = 0; j < TABLES_; ++j) {
      feature_correlation_[i][j] += int64_t(w[i]) * int64_t(w[j]);
    }
  }

  for (size_t i = 0; i < TABLES_; ++i) {
    feature_s1_[i] += w[i];
  }
  ++feature_n_;
//  inference_history_.print(w);
}

template<class Key>
const std::vector<typename HashedPerceptron<Key>::FeatureTable>&
HashedPerceptron<Key>::get_tables() const {
  return tables_;
}

template<typename Key>
void HashedPerceptron<Key>::clear_stats() {
  inference_stats_.clear();
}

}

#endif // PERCEPTRON_HPP
