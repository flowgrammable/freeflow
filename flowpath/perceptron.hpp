#ifndef PERCEPTRON_HPP
#define PERCEPTRON_HPP

#include <cstdint>
#include <vector>
#include <algorithm>
#include <numeric>
#include <sstream>

#include "types.hpp"


namespace entangle {

// Enables stats accumulation:
constexpr bool ENABLE_STATS = true;

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
  static constexpr size_t BITS_ = 7;  // resolution of single perceptron
  using Perceptron = ClampedInt<BITS_>;

  PerceptronTable(size_t elements);

  auto inference(Key k);
  void train(Key k, bool correlation);

  float dynamic_information(Key k) const;
  std::string print_stats() const;

private:
  const size_t MAX_;
  std::vector<Perceptron> table_;   // perceptron table for each feature

public:
  // Utilization Metadata:
  std::vector<int64_t> touch_inference_; // counts inference events
  std::vector<int64_t> touch_train_;     // counts train events
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
  using Feature = typename Key::value_type;
  using FeatureTable = PerceptronTable<Feature>;
  constexpr static size_t TABLES_ = std::tuple_size<Key>::value;
  using Weights = std::array<int, TABLES_>;

  HashedPerceptron(std::vector<size_t> elements);
  HashedPerceptron(size_t elements);

  auto inference(Key k);
  void reinforce(Key k, bool positive);   // positive/negative correlation
  void force_updates(bool force = true);

  std::string print_stats() const;
  void clear_stats();
  auto information_raw(Key tuple) const;

private:
  auto inference_raw(Key tuple);

  // Member Variables:
  std::vector<FeatureTable> tables_;  // 1 table per feature
  int64_t training_threshold_;
  int64_t inference_threshold_;
  bool force_update_ = false;

public:
  // Training Stats:
  int64_t train_total_ = 0;     // total reinforce calls
  int64_t train_occured_ = 0;   // reinforce calls actually followed through
  std::vector<inference_result> inference_history_;
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
auto PerceptronTable<Key>::inference(Key k) {
  assert(k < MAX_);
  if (ENABLE_STATS)
    ++touch_inference_[k];
  return table_[k];
}

template<typename Key>
void PerceptronTable<Key>::train(Key k, bool correlation) {
  assert(k < MAX_);
  if (ENABLE_STATS)
    ++touch_train_[k];
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
  return w;
}

template<class Key>
auto HashedPerceptron<Key>::information_raw(Key tuple) const {
  std::array<float, TABLES_> inf;
  for (size_t i = 0; i < TABLES_; ++i) {
    inf[i] = tables_[i].dynamic_information(tuple[i]);
  }
  return inf;
}

template<class Key>
auto HashedPerceptron<Key>::inference(Key tuple) {
  const Weights w = inference_raw(tuple);
  const int sum = std::accumulate(std::begin(w), std::end(w), 0);
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
    inference_history_.emplace_back(inference_result{confidence, saturation, agreement, information});
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
  const auto [prediction, sum] = inference(tuple);
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

// Prints stats over history of training events:
template<typename Key>
std::string HashedPerceptron<Key>::print_stats() const {
  std::stringstream ss;
  auto v = inference_history_;  // local mutable copy of huge stats vector...

  // Print utility of all weight tables:
  for (const auto& t : tables_) {
    // somehow accumulate / append stats for each table?
  }

  // Print stats over history of inference events:
  for (const auto [confidence, saturation, agreement, information] : inference_history_) {
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

    ss << v.size() << " inferences ("
       << inference_bias*100 << "% T)\t"
       << "[Confidence (" << median_confidence
       << "), Agreement (" << median_agreement
       << "), Saturation (" << median_saturation
       << "), Information (" << median_information
       << ")]" << std::endl;
  }
  return ss.str();
}

template<typename Key>
void HashedPerceptron<Key>::clear_stats() {
  inference_history_.clear();
}

}

#endif // PERCEPTRON_HPP
