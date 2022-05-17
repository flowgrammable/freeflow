#ifndef PERCEPTRON_HPP
#define PERCEPTRON_HPP

#include <cstdint>
#include <vector>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <random>

#include "types.hpp"
#include "drivers/pcap/util_io.hpp"

// Temporary global config hack:
#include "drivers/pcap/global_config.hpp"
extern struct global_config CONFIG;

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
  static constexpr bool ENABLE_TABLE_STATS = true; // inference count tracking

  static constexpr size_t BITS_ = 5;  // range of single perceptron
  using Perceptron = ClampedInt<BITS_>;

  PerceptronTable(size_t elements);
  void init();

  auto inference(Key k);
  void train(Key k, bool correlation);

  double recall_demand(Key k) const;
  const auto& get_table() const;
  void clear_stats();

  auto inference_internal(Key k);   // doesn't contribute to stats

private:
  const size_t MAX_;
  std::vector<Perceptron> table_;   // perceptron table for each feature

public:
  // Utilization Metadata:
  std::vector<int64_t> touch_inference_;  // counts inferences/perceptron
  std::vector<int64_t> touch_train_;      // counts trains/perceptron
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
  static constexpr bool ENABLE_TABLE_STATS = PerceptronTable<Key>::ENABLE_TABLE_STATS;
  static constexpr bool ENABLE_CORRELATION_ANALYSIS = true;
  static constexpr bool ENABLE_TABLE_DUMP = true;     // periodic weight dumps
//  static constexpr bool ENABLE_DYNAMIC_TRAINING_THRESHOLD = true;
  const bool ENABLE_DYNAMIC_TRAINING_THRESHOLD = {!CONFIG.ENABLE_Static_Threshold};

  using Feature = typename Key::value_type;
  using FeatureTable = PerceptronTable<Feature>;

  constexpr static size_t TABLES_ = std::tuple_size<Key>::value;
  static constexpr int64_t INFERENCE_MAX = (TABLES_-1) * (FeatureTable::Perceptron::MAX);
  static constexpr int64_t INFERENCE_MIN = (TABLES_-1) * (FeatureTable::Perceptron::MIN);
  static constexpr uint64_t INFERENCE_RANGE = (TABLES_-1) * (FeatureTable::Perceptron::RANGE);

  using Weights = std::array<int16_t, TABLES_>;

  HashedPerceptron(size_t elements);

  void init();  // Randomize all perceptron tables
  void force_updates(bool force = true);

  auto inference(Key k, bool tracked = true);
  auto reinforce(Key k, bool positive);   // positive/negative correlation
  double quantize(Key k, bool tracked = false);

  int64_t decision_threshold();
  std::array<int64_t, 2> training_threshold();
  std::array<int64_t, 2> calc_threshold(double ratio, bool set = false);  // (0.0, 1.0)

  std::string get_settings() const;
  void epoc_stats();
  void final_stats();
  void clear_stats();
  auto get_recall_demand(Key tuple) const;
  const std::vector<FeatureTable>& get_tables() const;

private:
  auto inference_tracked(Key tuple);
  auto inference_internal(Key tuple);
  void inference_event(Weights w);
  void training_event(Weights w, bool target);
  auto make_prediction(Weights w);

  // Member Variables:
  std::vector<FeatureTable> tables_;  // 1 table per feature
  // Tunable: related to bias of system:
  int64_t inference_threshold_ {};
//  int64_t inference_threshold_ = -100;

  double training_ratio_ {CONFIG.threshold};  // ensure between: {0,1}
  std::array<int64_t, 2> training_threshold_  // {-threshold, +threshold}
    {inference_threshold_ - (inference_threshold_ - INFERENCE_MIN) * training_ratio_,
     inference_threshold_ + (INFERENCE_MAX - inference_threshold_) * training_ratio_};
  ClampedInt<8>  threshold_pressure_;
  bool force_update_ {false};

public:
  // Training Stats:
  int64_t train_corrections_ {};    // corrections to incorrect predictions
  int64_t train_reinforcements_ {}; // reinforcements to correct predictions

private:
  // Feature Stats:
  using CorrelationMatrix = std::array<std::array<int64_t, TABLES_>, TABLES_>;
  CorrelationMatrix feature_correlation_ {{}};
  std::array<int64_t, TABLES_> feature_s1_ {}; // sum, power 1 (for std. deviation)
  int64_t feature_n_ {};  // number of inferences (for std. deviation)

  std::array<int64_t, TABLES_> feature_delta_ {}; // delta from confidence (correlation with 'true')
  int64_t feature_d_ {};  // number of training corrections (for correlatio with 'true')

  // Stat Output Files:
  CSV csv_fCorrelation_;  // mult+add (feature x feature)
  CSV csv_fS1_;           // sum (per feature)
  // S2 (feature^2 per feature) is identity in correlation matrix.
  CSV csv_fN_;            // inferences
  CSV csv_fDelta_;  // training correlation
  CSV csv_fD_;      // training
  std::array<CSV, TABLES_> csv_tables_;
  std::array<CSV, TABLES_> csv_stats_;
};


///////////////////////////////////////////////////////////////////////////////
/// PerceptronTable Implementation:
/// - Low-level table implementation.

template<typename Key>
PerceptronTable<Key>::PerceptronTable(size_t elements) :
  MAX_(elements), table_(elements),
  touch_inference_(elements, 0), touch_train_(elements,0) {
}

template<typename Key>
void PerceptronTable<Key>::init() {
  using IntType = typename Perceptron::IntType_t;
  std::random_device rd;
  std::default_random_engine e1(rd());
  std::uniform_int_distribution<IntType> uniform_dist(Perceptron::MIN, Perceptron::MAX);

  for (auto& p : table_) {
    p = uniform_dist(e1);
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
  if (ENABLE_TABLE_STATS) {
    ++touch_inference_[k];
//    inference_history_.push_back(ret.get());
  }
  return ret;
}

template<typename Key>
void PerceptronTable<Key>::train(Key k, bool correlation) {
  assert(k < MAX_);
  if (ENABLE_TABLE_STATS) {
    ++touch_train_[k];
  }
  correlation ? ++table_[k] : --table_[k];
}

// Returns rough estimate of recall demand for entry:
// - Ratio of training over total (inference ratio is inverse).
template<typename Key>
double PerceptronTable<Key>::recall_demand(Key k) const {
  assert(k < MAX_);
  return double(touch_train_[k])/double(touch_train_[k] + touch_inference_[k]);
}

template<typename Key>
const auto& PerceptronTable<Key>::get_table() const {
  return table_;
}

template<typename Key>
void PerceptronTable<Key>::clear_stats() {
  std::fill(touch_inference_.begin(), touch_inference_.end(), 0);
  std::fill(touch_train_.begin(), touch_train_.end(), 0);
}


///////////////////////////////////////////////////////////////////////////////
/// HashedPerceptron Implementation:
/// - High-level perceptron logic.
template<class Key>
HashedPerceptron<Key>::HashedPerceptron(size_t elements) :
  tables_(TABLES_, elements) {

  // set training & inference threshold based on # of tables?
//  size_t range = size_t(1) << (FeatureTable::BITS_-1);
//  training_threshold_ = TABLES_ * (FeatureTable::Perceptron::RANGE/2);
//  inference_threshold_ = 0; // Tunable: related to bias of system!
//  int64_t inferenceHeadroom = (inference_threshold_ >= 0) ?
//    (INFERENCE_MAX - inference_threshold_) : (INFERENCE_MIN - inference_threshold_);
//  training_threshold_ = inference_threshold_ + inferenceHeadroom/2;
//  training_threshold_ = calc_threshold(training_ratio_);
  std::cout << "Training Ratio: " << training_ratio_ << " ("
             << training_threshold_[0] << ","
             << training_threshold_[1] << ")" << std::endl;

  if (ENABLE_CORRELATION_ANALYSIS) {
    csv_fCorrelation_ = CSV("feature-correlation.csv");
    csv_fS1_ = CSV("feature-s1.csv");
    csv_fN_ = CSV("feature-n.csv");

    csv_fDelta_ = CSV("feature-delta.csv");
    csv_fD_ = CSV("feature-d.csv");
  }

  if (ENABLE_TABLE_DUMP) {
    for (size_t i = 0; i < TABLES_; i++) {
      std::string filename {"feature_table_" + std::to_string(i) + ".csv"};
      csv_tables_.at(i) = CSV(filename);
    }
  }

  if (ENABLE_TABLE_STATS) {
    for (size_t i = 0; i < TABLES_; i++) {
      std::string filename {"stats_table_" + std::to_string(i) + ".csv"};
      csv_stats_.at(i) = CSV(filename);
    }
  }
}

template<class Key>
void HashedPerceptron<Key>::force_updates(bool force) {
  force_update_ = force;
}

template<class Key>
void HashedPerceptron<Key>::init() {
  for (auto& t : tables_) {
    t.init();
  }
}

template<class Key>
auto HashedPerceptron<Key>::inference_tracked(Key tuple) {
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
  return w;
}

template<class Key>
auto HashedPerceptron<Key>::make_prediction(Weights w) {
  const int sum = std::accumulate(std::begin(w)+1, std::end(w), 0);
  const bool prediction = sum >= inference_threshold_;
  return std::make_pair(prediction, sum);
}

template<class Key>
auto HashedPerceptron<Key>::inference(Key tuple, bool tracked) {
  const Weights w = tracked ? inference_tracked(tuple) : inference_internal(tuple);
  const auto [prediction, sum] = make_prediction(w);
//  if (ENABLE_STATS) {
//    const int magnitude = std::abs(sum);
//    uint8_t confidence = (256*(magnitude - inference_threshold_))/INFERENCE_MAX;
//    int8_t saturation = ClampDown<int8_t>(magnitude - training_threshold_);
//    int8_t agreement = 0;
//    for (auto score : w) {
//      const int scaled_tr = inference_threshold_/std::size(w);
//      if ( score >= scaled_tr)
//        ++agreement;
//      else
//        --agreement;
//    }
//    auto information = [&]() {
//      // Calculate median of dynamic_information metric:
//      auto v = information_raw(tuple);  // FIXME!
//      auto median_it = std::begin(v) + std::size(v)/2;
//      std::nth_element(std::begin(v), median_it, std::end(v));
//      return int8_t(*median_it * 100);  // % cast to 8-bit int.
//    }();
//    inference_stats_.emplace_back(inference_result{confidence, saturation, agreement, information});
//  }
  return std::make_tuple(prediction, sum, w);
}

template<class Key>
auto HashedPerceptron<Key>::reinforce(Key tuple, bool target) {
  const Weights w = inference_internal(tuple);
  const auto [prediction, sum] = make_prediction(w);
  const bool incorrect = prediction != target;
  const bool reinforce = target ? sum < training_threshold_[1] : sum > training_threshold_[0];
  const bool update = incorrect || reinforce;

  if (ENABLE_CORRELATION_ANALYSIS) {
    training_event(w, target);
  }

  // Train/Reinforce perceptrons in each table:
  if (update || force_update_) {
    for (size_t i = 0; i < TABLES_; ++i) {
      tables_[i].train(tuple[i], target);
    }
  }

  // Dynamic Training Threshold Update Block:
  constexpr double UPDATE_STEP = 0.001;
  if (update) {
    if (incorrect) {  // correction
      ++train_corrections_;
      if (ENABLE_DYNAMIC_TRAINING_THRESHOLD) {
        ++threshold_pressure_;   // Saturating counter
        if (threshold_pressure_ == threshold_pressure_.MAX) {
          auto thr = calc_threshold(std::min(training_ratio_ + UPDATE_STEP, 1.0), true);
          threshold_pressure_ = 0;
          std::cout << "Training Ratio: " << training_ratio_ << " ("
                     << thr[0] << "," << thr[1] << ")" << std::endl;
        }
      }
    }
    else {  // else is reinforcement
      ++train_reinforcements_;
      if (ENABLE_DYNAMIC_TRAINING_THRESHOLD) {
        --threshold_pressure_;
        if (threshold_pressure_ == threshold_pressure_.MIN) {
          auto thr = calc_threshold(std::max(training_ratio_ - UPDATE_STEP, 0.0), true);
          threshold_pressure_ = 0;
          std::cout << "Training Ratio: " << training_ratio_ << " ("
                     << thr[0] << "," << thr[1] << ")" << std::endl;
        }
      }
    }
  }

//  if (ENABLE_TABLE_STATS) {
//    std::cout << "train: " << sum << " " << (update ? (incorrect ? "(train)" : "(reinforce)") : "(skip)") << std::endl;
//  }
  return std::make_tuple(update, incorrect, w);
}

template<class Key>
double HashedPerceptron<Key>::quantize(Key tuple, bool tracked) {
  auto [prediction, sum, w] = inference(tuple, tracked);
  return double(sum - INFERENCE_MIN)/INFERENCE_RANGE;
}

template<class Key>
int64_t HashedPerceptron<Key>::decision_threshold() {
  return inference_threshold_;
}

template<class Key>
std::array<int64_t, 2> HashedPerceptron<Key>::training_threshold() {
  return training_threshold_;
}

// (0.0, 1.0)
//template<class Key>
//int64_t HashedPerceptron<Key>::decision_threshold(float absolute, bool set) {
//  int64_t t = (INFERENCE_RANGE * absolute) + INFERENCE_MIN;
//  if (set) {
//    assert(t > INFERENCE_MIN && t < INFERENCE_MAX);
//    inference_threshold_ = t;
//  }
//  return t;
//}

// relative to decision threshold
template<class Key>
std::array<int64_t, 2> HashedPerceptron<Key>::calc_threshold(double ratio, bool set) {
  std::array<int64_t, 2> t =  // {-threshold, +threshold}
    {inference_threshold_ - (inference_threshold_ - INFERENCE_MIN) * ratio,
     inference_threshold_ + (INFERENCE_MAX - inference_threshold_) * ratio};
  if (set) {
    assert(ratio < 1.0);
    assert(t[0] > INFERENCE_MIN && t[0] < INFERENCE_MAX);
    assert(t[1] > INFERENCE_MIN && t[1] < INFERENCE_MAX);
    training_ratio_ = ratio;
    std::copy(std::begin(t), std::end(t), std::begin(training_threshold_));
  }
  return t;
}

template<class Key>
auto HashedPerceptron<Key>::get_recall_demand(Key tuple) const {
  std::array<double, TABLES_> inf;
  for (size_t i = 0; i < TABLES_; ++i) {
    inf[i] = tables_[i].recall_demand(tuple[i]);
  }
  return inf;
}

// Prints stats over history of training events:
template<typename Key>
std::string HashedPerceptron<Key>::get_settings() const {
  std::stringstream ss;
  ss << "Inference threshold: " << inference_threshold_
     << "/" << (inference_threshold_ >=0 ? INFERENCE_MAX : INFERENCE_MIN)
     << "\nTraining Ratio: " << training_ratio_ << "\n";
  if (force_update_) {
    ss << "Training threshold: disabled";
  }
  else {
    ss << "Training threshold: {" << training_threshold_[0]
       << ", " << training_threshold_[1] << "}";
  }
  return ss.str();
}

// Prints stats over history of training events:
template<typename Key>
void HashedPerceptron<Key>::epoc_stats() {
  if (ENABLE_CORRELATION_ANALYSIS) {
    for (auto r : feature_correlation_) {
      csv_fCorrelation_.append( a2t(r) );
    }
    csv_fS1_.append( a2t(feature_s1_) );
    csv_fN_.append( std::make_tuple(feature_n_) );

    csv_fDelta_.append( a2t(feature_delta_) );
    csv_fD_.append( std::make_tuple(feature_d_) );
  }
/*
  if (ENABLE_TABLE_DUMP) {
    for (size_t i = 0; i < TABLES_; i++) {
      const auto& tbl = tables_[i].get_table();
      std::vector<int16_t> table(tbl.size());
      std::transform(tbl.begin(), tbl.end(), table.begin(), [](const auto& ci) {
        return ci.get();
      });
      csv_tables_[i].append(table.begin(), table.end());
    }
  }

  if (ENABLE_TABLE_STATS) {
    for (size_t i = 0; i < TABLES_; i++) {
      const auto& inf = tables_[i].touch_inference_;
      const auto& train = tables_[i].touch_train_;
      csv_stats_[i].append(inf.begin(), inf.end());
      csv_stats_[i].append(train.begin(), train.end());
    }
  }
*/
}

// Prints stats over history of training events:
template<typename Key>
void HashedPerceptron<Key>::final_stats() {
  if (ENABLE_TABLE_DUMP) {
    for (size_t i = 0; i < TABLES_; i++) {
      const auto& tbl = tables_[i].get_table();
      std::vector<int16_t> table(tbl.size());
      std::transform(tbl.begin(), tbl.end(), table.begin(), [](const auto& ci) {
        return ci.get();
      });
      csv_tables_[i].append(table.begin(), table.end());
    }
  }

  if (ENABLE_TABLE_STATS) {
    for (size_t i = 0; i < TABLES_; i++) {
      const auto& inf = tables_[i].touch_inference_;
      const auto& train = tables_[i].touch_train_;
      csv_stats_[i].append(inf.begin(), inf.end());
      csv_stats_[i].append(train.begin(), train.end());
    }
  }
}

// Inference cross-correlation analysis:
// - Generate cross products between and sums of feature outputs (weights)
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
}

// Training cross-correlation analysis:
// - Correlate between correct direction and sums of feature outputs (weights)
template<class Key>
void HashedPerceptron<Key>::training_event(Weights w, bool target) {
  for (size_t i = 0; i < TABLES_; ++i) {
    feature_delta_[i] += w[i] * (target?1:-1);  // assumes w centered at 0...
  }
  ++feature_d_;
}


template<class Key>
const std::vector<typename HashedPerceptron<Key>::FeatureTable>&
HashedPerceptron<Key>::get_tables() const {
  return tables_;
}

template<typename Key>
void HashedPerceptron<Key>::clear_stats() {
  if (ENABLE_CORRELATION_ANALYSIS) {
    feature_correlation_ = {{}};
    feature_s1_ = {};
    feature_n_ = {};

    feature_delta_ = {};
    feature_d_ = {};
  }
/*
  if (ENABLE_TABLE_STATS) {
    for (size_t i = 0; i < TABLES_; i++) {
      tables_[i].clear_stats();
    }
  }
*/
}

}

#endif // PERCEPTRON_HPP
