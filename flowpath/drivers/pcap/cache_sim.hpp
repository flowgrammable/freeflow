#ifndef CACHE_SIM_HPP
#define CACHE_SIM_HPP

#include <vector>
#include <list>
#include <map>
#include <iostream>
#include <iterator>
#include <numeric>
#include <algorithm>
#include <string>
#include <sstream>
#include <random>

#include "types.hpp"
#include "util_container.hpp"
//#include "sim_lru.hpp"
#include "sim_min.hpp"
#include "perceptron.hpp"
#include "util_features.hpp"
#include "util_stats.hpp"
#include "global_config.hpp"

// Google's Abseil C++ library:
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"

extern struct global_config CONFIG;

// Global Random Number Generator:
std::mt19937 rE{std::random_device{}()};

// TODO: seperate Policy types for better config error catching
struct Policy {
  enum P {LRU=0, MRU=1, RANDOM=2, INVALID=255};
  Policy(P p) : p_(p) {}

  virtual bool operator==(Policy r) const {
    return p_ == r.p_;
  }

  P get() const {
    return p_;
  }
  size_t offset() const {
    return offset_;
  }

  // Members:
  private:
  P p_ = INVALID;
  size_t offset_ = 0;
};

struct InsertionPolicy : Policy {
  enum P {BYPASS=50, SHIP=51, HP_BYPASS=52};
  InsertionPolicy(P p) : Policy((Policy::P) p) {}

  virtual bool operator==(Policy r) const {
    return Policy::operator==(r);
  }
};

struct ReplacementPolicy : public Policy {
  enum P {BURST_LRU=100, SRRIP=101, SRRIP_CB=102, HP_LRU=103, HP_CONF=104};
  ReplacementPolicy(P p) : Policy((Policy::P) p) {}

  virtual bool operator==(Policy r) const {
    return Policy::operator==(r);
  }
};

bool operator==(Policy l, ReplacementPolicy r) {
  return l.operator==(r);
}
bool operator==(Policy l, InsertionPolicy r) {
  return l.operator==(r);
}


// Global type definitions:
//using Time = timespec;
//using MIN_Time = size_t;
//struct Reservation; // defined in sim_min.hpp
using BurstStats = std::vector<int>;  // mru burst hit counter


// Defines cache entry:
struct Stack_entry {
  Stack_entry() = delete;
//  Stack_entry(Reservation r) : res(r) {
//    features.h_ = hits;
//  }
  Stack_entry(Reservation r, Features f) : res{r}, features{f},
    hits{std::make_shared<BurstStats>(BurstStats{1})} {
    features.setBurstStats(hits);
    features.bless();
  }

  // Reference Prediciton Mechanism (RefCount & BurstCount):
  int refCount = 1;   // counts hits until eviction (equivalent to sum of HitStats)
//  int burstCount = 0; // counts bursts from non-MRU to MRU position (equivalent to HitStats.size())
  bool eol = false;   // indicates predicted to be end-of-lifetime

  // Re-reference Interval Prediction (RRIP):
//  int8_t RR_distance = 2;   // Range 0: near ... 3: distant;  Init: 2 slightly distant
  ClampedInt<3> RR_distance = ClampedInt<3>::MAX; // Range -2: distant  .. 1: recient

  // Signature-based Hit Predictor (SHiP):
  // signature (key, already stored)
  // re-reference boolean (equivalent to sum of HitStats > 1)

  // Statistics:
  Reservation res;
  std::shared_ptr<BurstStats> hits;

  // Perceptron Sampler Features:
  Features features;
};

// Defines Prediciton Table entry:
struct PT_entry {
  // Reference Prediciton Mechanism (RefCount & BurstCount):
  static const int64_t C_delta = 0;  // tolerable delta to avoid penalizing RC/BC; 0: for exact
  int BC_saved = -1;  // BurstCount (-1 indicates 1st insertion)
  int RC_saved = -1;  // RefCount (rolls over after 2.7s at line rate, 10G)
  ClampedInt<5> BC_confidence = -1; // 5-bit saturating counter.
  ClampedInt<5> RC_confidence = -1; // >0 indicates confident
//  short BC_confidence = -2;  // psudo confience metric (++, match; --, unmatched)
//  short RC_confidence = -2;

  // Signature-based Hit Predictor (SHiP):
  ClampedInt<2> SHiP_reuse = ClampedInt<2>::MAX;   // 2-bit saturating coutner (-2..1); -2: distant

  // Statistics:
//  HitStats history_hits;
//  std::vector<Reservation> history_res;
};

// Metadata of past prediction for Hashed Perceptron training:
struct Prediction {
  Prediction(Features f, bool pKeep, bool demand) : f_(f), pKeep_(pKeep), priority_(demand) {}
  Features f_;
  bool pKeep_;  // prediction (T: keep, F: evict)
  bool priority_;   // T: demand prediction, F: sampling prediction
};


// Collection of shared Hashed Perceptron table structures:
template<typename Key>
struct hpHandle {
  static constexpr bool RANDOM_INIT = true;

  hpHandle(size_t ptElements) : hp_(ptElements) {
    if (RANDOM_INIT) {
      hp_.init();
    }
  }

  entangle::HashedPerceptron<Features::FeatureType> hp_;

  // features for min eval:
  std::map<Key, Features> lastFeature_; // hack to evaluate MIN as training input.
  // this really should be tracked inside min datastructure itself.
};


////////////////////////////////////////////////////////////////////////////////
/// HISTORY TRAINER ////////////////////////////////////////////////////////////
//enum class Questions {Reuse, Bypass, COUNT};
//CSV keepLog("keepLog_"+CONFIG.simStartTime_str+".csv");
//CSV evictLog("evictLog_"+CONFIG.simStartTime_str+".csv");
constexpr bool DUMP_PREDICTIONS = false;
constexpr bool DUMP_TRAINING = false;
CSV bypassPred;
CSV evictPred;
CSV trainingEvents;

// Training instantiation track training state intended for sample sets
// and share PerceptronTable state.
template<typename Key>
class HistoryTrainer {
public:
  using Entry = std::pair<Key, Prediction>;
  const size_t pKEEP_DEPTH;
  const size_t pEVICT_DEPTH;

  HistoryTrainer(entangle::HashedPerceptron<Features::FeatureType>& hp,
                 size_t pEvictDepth, size_t pKeepDepth) :
    hp_(hp), pEVICT_DEPTH(pEvictDepth), pKEEP_DEPTH(pKeepDepth)
  {
    pEvictHistory_.reserve(pEvictDepth);
    pKeepHistory_.reserve(pKeepDepth);
    if (DUMP_PREDICTIONS) {
      bypassPred = CSV("bypassPred.csv");
      evictPred = CSV("evictPred.csv");
    }
    if (DUMP_TRAINING) {
      trainingEvents = CSV("trainingEvents.csv");
    }
  }

  // Legacy hp event prediction interface:
  // TODO: is there a clean way to seperate questions asked of HP..?
  void touch(Key k, Features f);
  void prediction(Key k, Features f, bool keep, bool demand = true);
  void reinforce(Key k, Prediction p, bool touched);

private:
  entangle::HashedPerceptron<Features::FeatureType>& hp_;
  std::vector<Entry> pKeepHistory_;
  std::vector<Entry> pEvictHistory_;
};


// Searches history buffers for prediction error on packet event.
// - Optionally samples accesses to provide training information to predictor.
template<typename Key>
void HistoryTrainer<Key>::touch(Key k, Features f) {
  { // Search keep prediction history for entry:
    auto it = std::find_if(std::begin(pKeepHistory_), std::end(pKeepHistory_),
      [k](const std::pair<Key, Prediction>& p) {
        return p.first == k;
      }
    );
    if (it != std::end(pKeepHistory_)) {
      // Found correct Keep prediction: reinforce keep:
      reinforce(k, it->second, true);
      pKeepHistory_.erase(it);
    }
  }

  { // Search evict prediction history for entry:
    auto it = std::find_if(std::begin(pEvictHistory_), std::end(pEvictHistory_),
      [k](const std::pair<Key, Prediction>& p) {
        return p.first == k;
      }
    );
    if (it != std::end(pEvictHistory_)) {
      // Found incorrect Evict prediction: train keep:
      reinforce(k, it->second, true);
      pEvictHistory_.erase(it);
    }
  }
}

// Takes note of prediction in history for later confirmation:
template<typename Key>
void HistoryTrainer<Key>::prediction(Key k, Features f, bool keep, bool demand) {
  // Take note of prediction:
  Prediction p{f, keep, demand};

  // OPTIONAL: only insert sample if queue does not contain flow?
  if (keep) {
    if (pKeepHistory_.size() >= pKEEP_DEPTH) {
      // Pop last Keep prediction: train evict (never touched)
      reinforce(k, pKeepHistory_.back().second, false);
      pKeepHistory_.pop_back();
    }
//    pKeepHistory_.emplace_back(k, p);
    pKeepHistory_.emplace(std::begin(pKeepHistory_), std::piecewise_construct,
                          std::forward_as_tuple(k),
                          std::forward_as_tuple(p) );
  }
  else {
    if (pEvictHistory_.size() >= pEVICT_DEPTH) {
      // Pop last Evict prediction: reinforce evict (never touched)
      reinforce(k, pEvictHistory_.back().second, false);
      pEvictHistory_.pop_back();
    }
//    pEvictHistory_.emplace_back(k, p);
    pEvictHistory_.emplace(std::begin(pEvictHistory_), std::piecewise_construct,
                          std::forward_as_tuple(k),
                          std::forward_as_tuple(p) );
  }
}

// Performs actual reinforcement:
template<typename Key>
void HistoryTrainer<Key>::reinforce(Key k, Prediction p, bool touched) {
  const auto features = p.f_.gather(true);
  auto [reinforced, weights] = hp_.reinforce(features, touched);
  if (DUMP_TRAINING && reinforced) {
    const char direction = touched ? '+' : '-';
    trainingEvents.append(
      std::tuple_cat(std::make_tuple(direction, k), weights, features)
    );
  }
}


////////////////////////////////////////////////////////////////////////////////
/// BELADY TRAINER /////////////////////////////////////////////////////////////
// Manages Belady training state fit for sample sets.
template<typename Key>
class BeladyTrainer {
public:
  BeladyTrainer(entangle::HashedPerceptron<Features::FeatureType>& hp,
                 size_t entries) : hp_(hp), belady_(entries) {}

  void touch(Key k, Features f, Time t);
  void reinforce(Key k, Features f, bool keep);

private:
  entangle::HashedPerceptron<Features::FeatureType>& hp_;
  SimMIN<Key> belady_;
  std::map<Key, Features> fmap_;
};


template<typename Key>
void BeladyTrainer<Key>::touch(Key k, Features f, Time t) {
  constexpr bool ENABLE_Belady_EvictSet_Training = true;
  constexpr bool ENABLE_Belady_KeepSet_Training = false;

  // Consult Belady's algorithm for training insight:
  bool beladyHit = belady_.update(k, t);
  if (beladyHit) {
    // Potentially new information on capacity hit:
    auto [evictSet, keepSet] = belady_.evictions();
    if (ENABLE_Belady_EvictSet_Training) {
      for (Key v : evictSet) {
        // TRYME: (Bypass) only reinforce if eviction was !touched...?
        const Features& ef = fmap_.at(v);
        reinforce(v, ef, false);
//        hp_.reinforce(ef.gather(true), false);  // reinforce as non-cachable
      }
     }
    if (ENABLE_Belady_KeepSet_Training) {
      for (Key v : keepSet) {
        // TRYME: only reinforce if eviction was touched since advance...?
        const Features& kf = fmap_.at(v);
        reinforce(v, kf, true);
//        hp_.reinforce(kf.gather(true), true);   // reinforce as cachable
      }
    }
  }

  // Update feature for t+1:
//  fmap_.insert_or_assign(k, f); // replace last feature
  auto [it, inserted] = fmap_.insert(k, f);
  if (!inserted) {
    it->merge(f);
  }
}


// Performs actual reinforcement:
template<typename Key>
void BeladyTrainer<Key>::reinforce(Key k, Features f, bool keep) {
  const auto features = f.gather(true);
  auto [reinforced, weights] = hp_.reinforce(features, keep);
  if (DUMP_TRAINING && reinforced) {
    const char direction = keep ? '+' : '-';
    trainingEvents.append(
      std::tuple_cat(std::make_tuple(direction, k), weights, features)
    );
  }
}


////////////////////////////////////////////////////////////////////////////////
/// ASSOCIATIVE SET ////////////////////////////////////////////////////////////
template<typename Key>
class AssociativeSet {
public:
  using Stack = std::list< std::pair<Key, Stack_entry> >;
  using Stack_it = typename Stack::iterator;
  using Stack_value = typename Stack::value_type;
  using Evict_t = std::tuple<Key, Reservation, std::shared_ptr<BurstStats>>;

  using PatternTable = std::map<Key, PT_entry>;
//  using RejectTable = std::list< std::pair<Key, Features> >;

  AssociativeSet(size_t entries, hpHandle<Key>& hp,
                 Policy insert = Policy(Policy::MRU),
                 Policy replace = Policy(Policy::LRU));

  auto insert(Key k, const Time& t, Features);
  auto update(Key k, const Time& t, Features);
  bool demote(Key k);
  auto flush(Key k);
  auto flush();

  auto insert_find(Key k);
  auto replace_find();

  void set_insert_policy(Policy insert);
  void set_replacement_policy(Policy replace);

  const auto& get_stack() const {return stack_;}

  // Stats:
  int64_t get_hits() const {return hits_;}
  int64_t get_capacity_miss() const {return capacityMiss_;}
  int64_t get_compulsory_miss() const {return compulsoryMiss_;}

  // Replacement Stats:
  int64_t get_replacement_lru() const {return replacementLRU_;}
  int64_t get_replacement_early() const {return replacementEarly_;}
  int64_t get_prediction_bc() const {return predictionBC_;}
  int64_t get_prediction_rc() const {return predictionRC_;}
  int64_t get_prediction_rc_better() const {return predictionRC_better_;}
  int64_t get_prediction_too_early() const {return eagerEarlyReplacement_;}
  int64_t get_insert_predict_distant() const {return insertPedictDistant_;}
  int64_t get_replacement_reciency() const {return replacementReciency_;}
  std::pair<int64_t, int64_t> get_prediction_hp() const
    {return std::make_pair(predictionHPBypass_, predictionHPEvict_);}

  // Stack comparison for debugging:
  bool operator==(const AssociativeSet<Key>& other);
  bool operator==(const Stack& other);

private:
  auto internal_insert(const Key& k, const Time& t, Features);

  void event_eviction(Stack_it eviction);
  void event_mru_demotion(Stack_it demoted);
  void event_mru_hit(Stack_it hit);

  // Generic Policies:
  auto find_MRU(size_t pos);
  auto find_LRU(size_t pos);
  auto find_LIR(size_t pos);
  auto find_MIR(size_t pos);
  auto find_Random(size_t pos);

  //  Replacement Policies:
  auto find_RRIP_Distance(size_t pos);
  auto find_Expired(size_t pos);

  // Insertion Policies:
  auto find_SHiP_Reuse(Key k, size_t pos);
  auto find_Bypass(Key k, size_t pos);

  // Way Config:
  const size_t MAX_;
  Policy p_insert_;
  Policy p_replace_;

  // Way Containers:
  Stack stack_;   // Reciency stack: MRU at front, LRU at back
  absl::flat_hash_map<Key, Stack_it> lookup_;

  // Hit Stats:
  int64_t hits_ = 0;
  int64_t compulsoryMiss_ = 0;
  int64_t capacityMiss_ = 0;
  // Replacement Predictor Stats
  int64_t replacementLRU_ = 0;
  int64_t replacementEarly_ = 0;
  int64_t predictionBC_ = 0;
  int64_t predictionRC_ = 0;
  int64_t predictionHPEvict_ = 0;
  int64_t predictionHPBypass_ = 0;
  int64_t predictionRC_better_ = 0;
  int64_t eagerEarlyReplacement_ = 0;
  // Insert Predictor Stats
  int64_t insertPedictDistant_ = 0;
  int64_t replacementReciency_ = 0;

  // Burst Predictor:
  std::map<Key, PT_entry> pt_;  // Prediction metadata independent of cache.

  // Shared Prediction State:
//  entangle::HashedPerceptron<Features::FeatureType>& hp_;
  hpHandle<Key>& hp_; // Global hashed perceptron state owned by CacheSim.
  HistoryTrainer<Key> hpHistory_; // Distributed history-based perceptron trainer
  BeladyTrainer<Key> hpBelady_; // Distributed belady perceptron trainer

  // Hashed Perceptron Configs:
//  const bool ENABLE_Perceptron_DeadBlock_Prediction_on_Touch = true;
  const bool ENABLE_Perceptron_DeadBlock_Prediction_on_MRU_Demotion = false;
  const bool ENABLE_Perceptron_DeadBlock_Prediction_on_Touch = !ENABLE_Perceptron_DeadBlock_Prediction_on_MRU_Demotion;
//  const bool ENABLE_Perceptron_Bypass_Prediction = true;

  // Positive Reinforcement:
  const bool ENABLE_Hit_Training = false;           // (+) on every cache hit
  const bool ENABLE_MRU_Promotion_Training = false;  // (+) when promoted to MRU
  const bool ENABLE_Belady_KeepSet_Training = false; // (+) for Belady's keepSet

  // Negative Reinforcement:
  const bool ENABLE_Eviction_Training = false;  // (-) on eviction without touch
  const bool ENABLE_Belady_EvictSet_Training = false; // (-) for Belady evictSet

  // Corrective Training:
  const bool ENABLE_History_Training = true;    // (+/-) prediction feedback delay loop
  const bool ENABLE_EoL_Hit_Correction = true;  // (+) on incorrect eol mark

  const bool ENABLE_Belady_Training =
      ENABLE_Belady_EvictSet_Training || ENABLE_Belady_KeepSet_Training;

  // Belady's algorithm:
  const bool ENABLE_Belady = false || ENABLE_Belady_Training;
  SimMIN<Key> belady_;
  int64_t perfectHits_ = 0;       // count when cache and belady agreed
  int64_t perfectEvictions_ = 0;  // count when chosen eviciton was considered perfect
};


template<typename Key>
class CacheSim {
public:
  using Time = timespec;  // maybe throw out?
  using Stack_value = typename AssociativeSet<Key>::Stack_value;
//  using Reservation = typename AssociativeSet<Key>::Reservation;
//  using HitStats = typename AssociativeSet<Key>::HitStats;

  CacheSim(size_t entries, size_t ways = 0);
  auto insert(const Key& k, const Time& t, Features f);
  auto update(const Key& k, const Time& t, Features f);
  bool demote(const Key& k);
  auto flush(const Key& k);
  auto flush();

  size_t get_size() const { return MAX_; }
  size_t get_associativity() const { return MAX_/num_sets(); }
  size_t num_sets() const {
    if (!sets_.empty())
      return sets_.size();
    return 1;
  }
  AssociativeSet<Key>& get_set(size_t i) const { return sets_.at(i); }

  void set_insert_policy(Policy insert);
  void set_replacement_policy(Policy insert);

  // Stats:
  int64_t get_hits() const;
  int64_t get_compulsory_miss() const;
  std::tuple<int64_t,int64_t,int64_t> get_misses() const; // {compulsory, capacity, conflict}
  int64_t get_pure_capacity_miss() const;   // does not take into account conflict hits..
  int64_t get_pure_conflict_misses() const; // does not take into account conflict hits...

  // Fully-Associative Stats:
  int64_t get_fa_hits() const;
  int64_t get_fa_capacity_miss() const;  
  int64_t get_pure_conflict_hits() const;

  // Replacement Stats:
  int64_t get_replacements_lru() const;
  int64_t get_replacements_early() const;
  int64_t get_prediction_bc() const;
  int64_t get_prediction_rc() const;
  int64_t get_prediction_rc_better() const;
  int64_t get_replacements_eager() const;
  int64_t get_insert_predict_distant() const;
  double get_replacement_reciency() const;
  std::pair<int64_t, int64_t> get_prediction_hp() const;

  std::string print_stats();
  hpHandle<Key>& get_hp_handle();

private:
  // Cache Config:
  const size_t MAX_;  // Total cache elements

  // Shared Prediction State:
  hpHandle<Key> hp_;
//  using FeatureType = std::result_of<decltype(&Features::gather)>::type;
//  entangle::HashedPerceptron<Features::FeatureType> hp_;
//  std::vector< std::pair<Key,Features> > hp_NegativeHistory_;
//  std::vector< std::pair<Key,Features> > hp_PositiveHistory_;

  // Cache Containers:
  AssociativeSet<Key> fa_ref_;  // Conflict reference: Fully Associative
  std::vector< AssociativeSet<Key> > sets_;  // Vector of associative sets

  // Sanity check stats:
  int64_t capacityMiss_ = 0;
  int64_t conflictMiss_ = 0;
  int64_t conflictHit_ = 0;
};


/////////////////////////////////////
/// AssociativeSet Implementation ///
/////////////////////////////////////
template<typename Key>
AssociativeSet<Key>::AssociativeSet(size_t entries,
    hpHandle<Key>& hp, Policy insert, Policy replace) :
  MAX_(entries), lookup_(entries), p_insert_(insert), p_replace_(replace),
  belady_(entries), hp_(hp),
  hpHistory_(hp.hp_, entries, entries), hpBelady_(hp.hp_, entries) {}


// Key has been created (first occurance)
template<typename Key>
auto AssociativeSet<Key>::insert(Key k, const Time& t, Features f) {
  compulsoryMiss_++;

  // Consult Belady's Algorithm:
  if (ENABLE_Belady) {
    belady_.insert(k, t);
    if (ENABLE_Belady_Training) {
      hp_.lastFeature_.insert_or_assign(k, f); // replace last feature
    }
  }

  // Consult Hashed Perceptron on bypass:
  // TODO: Add Insertion policy for HP...
  if (p_insert_ == InsertionPolicy::HP_BYPASS) {
    auto [keep, weights] = hp_.hp_.inference(f.gather(true));
    if (ENABLE_History_Training) {
      hpHistory_.prediction(k, f, keep);
    }
    if (!keep) {
      ++predictionHPBypass_;
      if (DUMP_PREDICTIONS) {
        bypassPred.append( std::tuple_cat(std::make_tuple(k), weights, f.gather(true)) );
      }
      return std::optional<Evict_t>{};  // bypass
    }
  }

  return internal_insert(k, t, f);
}


// Key has been accessed again (not first occurance)
// - Determine if hit or capacity conflict.
// - Returns {hit/miss, eviction (if necessary)}
template<typename Key>
auto AssociativeSet<Key>::update(Key k, const Time& t, Features f) {
  // Consult Belady's Algorithm:
  bool beladyHit = false;
  std::set<Key> beladyVictims;
  if (ENABLE_Belady) {
    beladyHit = belady_.update(k, t);
    if (beladyHit) {
      // Potentially new information on capacity hit:
      auto [evictSet, keepSet] = belady_.evictions();
      if (ENABLE_Belady_EvictSet_Training) {
        for (Key v : evictSet) {
          const Features& ef = hp_.lastFeature_.at(v);
          hp_.hp_.reinforce(ef.gather(true), false);  // reinforce as non-cachable
        }
      }
      if (ENABLE_Belady_KeepSet_Training) {
        for (Key v : keepSet) {
          const Features& kf = hp_.lastFeature_.at(v);
          hp_.hp_.reinforce(kf.gather(true), true);   // reinforce as cachable
        }
      }
      beladyVictims.swap(evictSet);
    }
  }

  // Update/Sample Hashed Perceptron:
  if (ENABLE_History_Training && (p_insert_ == InsertionPolicy::HP_BYPASS || p_replace_ == ReplacementPolicy::HP_LRU) ) {
    hpHistory_.touch(k, f);
  }

  // Cache Simulation:
  MIN_Time column = compulsoryMiss_ + capacityMiss_;  // Comparable to Min's t
  auto status = lookup_.find(k);
  if (status != lookup_.end()) {
    hits_++;
    Stack_it it = status->second;
    Stack_entry& entry = std::get<Stack_entry>(*it);
    Reservation& res = entry.res;
    BurstStats& hitStats = *(entry.hits);

    ++entry.refCount;
    res.extend(t, column);

    // TODO: factor out into promotion function if modularity is needed:
    if (it == stack_.begin()) {
      hitStats.back()++; // at MRU (continuting burst)
      event_mru_hit(it);
    }
    else {
      // Update LRU Stack:
      hitStats.emplace_back(1);  // new burst
      Stack_it demotion = stack_.begin(); // let naturally demote by 1 position
      stack_.splice(stack_.begin(), stack_, it, std::next(it));
      event_mru_demotion(demotion);
    }

    if (ENABLE_Belady && beladyHit) {
      ++perfectHits_;
    }
    if (ENABLE_Belady_Training) {
      hp_.lastFeature_.insert_or_assign(k, f);  // update last feature
    }
    if (ENABLE_Hit_Training) {
      const Features& df = entry.features;
      hp_.hp_.reinforce(df.gather(), true);  // reinforce reciency on every hit
    }

//    entry.features = f; // update features to latest touch
    entry.features.merge(f); // update features to latest touch

    // Consult Hashed Perceptron:
    if (ENABLE_Perceptron_DeadBlock_Prediction_on_Touch &&
        p_replace_ == ReplacementPolicy::HP_LRU) {
      const Features& df = entry.features;
      auto [keep, weights] = hp_.hp_.inference(df.gather());
      if (!keep) {
        ++predictionHPEvict_;
        entry.eol = true;
        if (DUMP_PREDICTIONS) {
          evictPred.append( std::tuple_cat(std::make_tuple(k), weights, df.gather()) );
        }
      }
      if (ENABLE_History_Training) {
        hpHistory_.prediction(k, df, keep);
      }
    }

    return std::make_pair(true, std::optional<Evict_t>{});  // hit
  }
  else {
    capacityMiss_++;

    // Consult Hashed Perceptron on bypass:
    // TODO: Add Insertion policy for HP...
    if (p_insert_ == InsertionPolicy::HP_BYPASS) {
      auto [keep, weights] = hp_.hp_.inference(f.gather(true));
      if (ENABLE_History_Training) {
        hpHistory_.prediction(k, f, keep);
      }
      if (!keep) {
        ++predictionHPBypass_;
        if (DUMP_PREDICTIONS) {
          bypassPred.append( std::tuple_cat(std::make_tuple(k), weights, f.gather(true)) );
        }
        return std::make_pair(false, std::optional<Evict_t>{});  // bypass
      }
    }
    auto victim = internal_insert(k, t, f);
    return std::make_pair(false, victim);  // miss
  }
}


// Internal function to manage insertion into a generic reciency stack.
// - Returns evicted std::pair<Key, Reservation> if stack >= MAX.
// - Returns default constructed std::pair<Key(0), Reservation()> if < MAX
template<typename Key>
auto AssociativeSet<Key>::internal_insert(const Key& k, const Time& t, Features f) {
  MIN_Time column = compulsoryMiss_ + capacityMiss_;  // Comparable to Min's t
//  Stack_entry new_e = Stack_entry(Reservation(column,column), f);
//  Stack_entry new_e = Stack_entry(Reservation_t(t, t), f);
  Stack_entry new_e = Stack_entry(Reservation(t, column), f);

  // Replacement Policy:
  std::optional<Evict_t> eviction;
  if (stack_.size() >= MAX_) {
    auto victim_it = replace_find();
    Key victim_key = std::get<Key>(*victim_it);
    Stack_entry& victim_e = std::get<Stack_entry>(*victim_it);

    // Note: calling event_eviction before evicting to preserve stack...
    event_eviction(victim_it);
    lookup_.erase(victim_key);

    eviction = Evict_t(victim_key, std::move(victim_e.res), victim_e.hits);
    stack_.erase(victim_it);

    // Consult Belady's Algorithm:
//    // ISSUE: perfect knowledge is delayed...  how to sync?
//    auto it = beladyEvictions_.find(victim_key);
//    if (it != beladyEvictions_.end()) {
//      ++perfectEvictions_;
//      beladyEvictions_.erase(it);
//    }
//    else {
//      ++imperfectEvictions_;
//      // ISSUE: not keeping track of set of imperfect evictions...
//    }
  }
  // else evicted (std::optional) is left default constructed.

  // Insert Policy:
  auto insert_it = insert_find(k);
  auto item_it = stack_.emplace(insert_it, Stack_value(k, std::move(new_e)));
  lookup_.insert( std::make_pair(k,item_it) );
  assert(lookup_.size() == stack_.size());
  return eviction;
}


// Flushes entry (indexed by Key):
template<typename Key>
auto AssociativeSet<Key>::flush(Key k) {
  auto status = lookup_.find(k);
  if (status != lookup_.end()) {
    Stack_it it = status->second;
    return std::optional<Stack_value>(*it);
  }
  else {
    return std::optional<Stack_value>{};
  }
  // TODO: Delete from lookup and stack...
}

// Flushes entire cache:
template<typename Key>
auto AssociativeSet<Key>::flush() {
  Stack s;
  std::swap(stack_, s);
  lookup_.clear();
  return s;
}


// Eviction event triggers this function to note metadata as needed.
// - Called just prior to eviction to preserve stack for analysis
template<typename Key>
void AssociativeSet<Key>::event_eviction(Stack_it eviction) {
  const auto& [key, entry] = *eviction;
  const BurstStats& hitStats = *(entry.hits);
  const Reservation& res = entry.res;

  // Calculate burst & reference count:
  const int burstCount = hitStats.size();

  // Create/Update Pattern Table entry.
  PT_entry& pte = pt_[key];

  // Update BurstCount pattern table:
  const auto BC_delta = pte.BC_saved - burstCount;   // +: less pkts than saved
  if (BC_delta == 0) {
    ++pte.BC_confidence;
  }
  else {
    if (abs(BC_delta) > PT_entry::C_delta) {
      --pte.BC_confidence;
      pte.BC_saved = burstCount;
    }
    else {
      pte.BC_saved = std::min(burstCount, pte.BC_saved);
    }
  }

  // Update ReferenceCount pattern table:
  const auto RC_delta = pte.RC_saved - entry.refCount; // +: less pkts than saved
  if (RC_delta == 0) {
    ++pte.RC_confidence;
  }
  else {
    if (abs(RC_delta) > PT_entry::C_delta) {
      --pte.RC_confidence;
      pte.RC_saved = entry.refCount;
    }
    else {
      pte.RC_saved = std::min(entry.refCount, pte.RC_saved);
    }
  }

  // Train Hashed Perceptron:
  if (ENABLE_Eviction_Training) {
    const Features& df = entry.features;
    // Since entry.features is always last feature set,
    // possibly can reinforce evict on last packet unconditionally...
    if (entry.refCount <= 1) {
      hp_.hp_.reinforce(df.gather(), false);  // reinforce as non-cachable
    }
  }

  // Update SHIP re-reference interval on eviction:
  if (entry.refCount == 1) {
    --pte.SHiP_reuse;     // Not re-referenced before eviction, decrement.
  }
  else {
    ++pte.SHiP_reuse;   // BurstCount like increment.
  }

  // Update statistics:
//  auto MINLifetime = res.second - res.first;  // how many misses did this element survive?
//  pte.history_hits.insert(pte.history_hits.end(), hitStats.begin(), hitStats.end());
//  pte.history_res.push_back(res);
}


// Reciency stack change event triggers this function to note metadata as needed.
// - Called post stack adjustment with iterator pointing to demoted entry.
template<typename Key>
void AssociativeSet<Key>::event_mru_demotion(Stack_it demoted) {
  { // Take notes on demoted MRU:
    auto& [k, entry] = *demoted;
    const BurstStats& hitStats = *(entry.hits);

    if (p_replace_ == ReplacementPolicy::BURST_LRU) {
      const int64_t burstCount = hitStats.size();
      PT_entry& pte = pt_[k];
  ///    assert(!entry.eol); // sanity check
      if (pte.BC_confidence >= 0 && burstCount >= pte.BC_saved) {
        predictionBC_++;
  //      entry.eol = true;   // mark as candidate for replacement
      }
      if (pte.RC_confidence >= 0 && entry.refCount >= pte.RC_saved) {
  //      if (!entry.eol) {   // hack to count if RC > BC
  //        predictionRC_better_++;
  //      }
        predictionRC_++;
        entry.eol = true;   // mark as candidate for replacement
      }
    }

    // Consult Belady's Algorithm:
//    if (ENABLE_Belady_Training) {
//      const Features& df = entry.features;
//      auto [keep, value] = hp_.pt_.inference(df.gather());
//      if (!keep) {
//        ++predictionHPEvict_;
//        entry.eol = true;
//      }
//    }

    // Consult Hashed Perceptron:
    if (ENABLE_Perceptron_DeadBlock_Prediction_on_MRU_Demotion &&
        p_replace_ == ReplacementPolicy::HP_LRU) {
      const Features& df = entry.features;
      auto [keep, weights] = hp_.hp_.inference(df.gather());
      if (!keep) {
        ++predictionHPEvict_;
        entry.eol = true;
        if (DUMP_PREDICTIONS) {
          evictPred.append( std::tuple_cat(std::make_tuple(k), weights, df.gather()) );
        }
      }
      if (ENABLE_History_Training) {
        hpHistory_.prediction(k, df, keep);
      }
    }
  }

  { // Take notes on new MRU:
    Stack_it mru = stack_.begin();
    auto& [key, entry] = *mru;
    // Ensure not marked for replacement:
    if (entry.eol) {
      eagerEarlyReplacement_++;
      entry.eol = false;
      if (ENABLE_EoL_Hit_Correction) {
        // Note, Positive Eviction Training requires entry features to be updated after this call!
        const Features& df = entry.features;
        hp_.hp_.reinforce(df.gather(), true);   // reinforce as cachable
      }
    }
    // Update RRIP re-reference interval on promition:
    if (p_replace_ == ReplacementPolicy::SRRIP_CB && entry.refCount > 1) {
      /* This threshold is rather arbitrary and 'easy' to achive.
       * Starting from whats in RRIP/SHiP paper. */
      ++entry.RR_distance;  // Re-referenced within Burst.
    }
  }
}


// MRU hit event triggers this function to note metadata as needed.
// - Called with iterator pointing to hit entry.
template<typename Key>
void AssociativeSet<Key>::event_mru_hit(Stack_it hit) {
  // Take notes on hit:
  auto& [key, entry] = *hit;

  // Update RRIP re-reference interval on hit:
  if (p_replace_ == ReplacementPolicy::SRRIP) {
    /* This threshold is rather arbitrary and 'easy' to achive.
     * Starting from whats in RRIP/SHiP paper. */
    ++entry.RR_distance;  // Re-referenced on hit.
  }

  if (ENABLE_MRU_Promotion_Training) {
    // Note, Positive Eviction Training requires entry features to be updated after this call!
    const Features& df = entry.features;
    hp_.hp_.reinforce(df.gather(), true);   // reinforce as cachable
  }
}




template<typename Key>
auto AssociativeSet<Key>::find_MRU(size_t pos) {
  if (stack_.empty()) {
    return stack_.end();
  }
  if (stack_.size() <= pos) {
    return std::next(stack_.rbegin()).base();
  }
  return std::next(stack_.begin(), pos);
}


template<typename Key>
auto AssociativeSet<Key>::find_LRU(size_t pos) {
  if (stack_.empty()) {
    return stack_.end();
  }
  if (stack_.size() <= pos) {
    return std::next(stack_.rbegin()).base();
  }
  return std::next(stack_.rbegin(), pos+1).base();
}


// Least Inferred Reuse (LIR)
// - Sort reciency stack by smallest raw hashed-perceptron as of last inference.
template<typename Key>
auto AssociativeSet<Key>::find_LIR(size_t pos) {
  if (stack_.empty()) {
    return stack_.end();
  }
  if (stack_.size() <= pos) {
    return std::next(stack_.rbegin()).base();
  }
//  std::array<int> infStack(MAX_);
  return std::next(stack_.rbegin(), pos+1).base();
}


template<typename Key>
auto AssociativeSet<Key>::find_Random(size_t pos) {
  if (stack_.empty()) {
    return stack_.end();
  }
  if (stack_.size() <= pos) {
    return std::next(stack_.rbegin()).base();
  }
  std::uniform_int_distribution<size_t> dist(0, MAX_ - pos);
  size_t n = dist(rE);
  return std::next(stack_.begin(), n);
}


// Insertion Policies:
template<typename Key>
auto AssociativeSet<Key>::find_SHiP_Reuse(Key k, size_t pos) {
  // Edge cases:
  if (stack_.empty()) {
    return stack_.end();
  }
  if (stack_.size() <= pos) {
    return std::next(stack_.rbegin()).base();
  }

  PT_entry& pte = pt_[k];
  auto offset = pte.SHiP_reuse.get() - decltype(pte.SHiP_reuse)::MIN;
  assert(offset == pte.SHiP_reuse.unsignedDistance());
  if (pte.SHiP_reuse == decltype(pte.SHiP_reuse)::MIN) {
    // Predict distant reuse, insert at LRU:
    ++insertPedictDistant_;
    return std::next(stack_.rbegin(), pos+1).base();
  }
//  size_t insert_pos = stack_.size()/2;
//  return std::next(stack_.begin(), insert_pos); // default insert to middle of stack.
  return std::next(stack_.begin(), pos);  // default insert at MRU
}

template<typename Key>
auto AssociativeSet<Key>::find_Bypass(Key k, size_t pos) {
  // Edge cases:
  if (stack_.empty()) {
    return stack_.end();
  }
  if (stack_.size() <= pos) {
    return std::next(stack_.rbegin()).base();
  }

  if (pt_.count(k) == 0) {
    // Not seen before; predict distant reuse, insert at LRU:
    ++insertPedictDistant_;
    return std::next(stack_.rbegin(), pos+1).base();
  }
  return std::next(stack_.begin(), pos);  // default insert at MRU
}


//  Replacement Policies:
template<typename Key>
auto AssociativeSet<Key>::find_Expired(size_t pos) {
  // Finds oldest candidate marked expired:
  auto lifetime_expired = [](const Stack_value& s) {
    return std::get<Stack_entry>(s).eol;
  };

  // Find first (oldest referenced) replacement candidate:
  auto eol_rit = std::find_if(stack_.rbegin(), stack_.rend(), lifetime_expired);
  // Convert to forward iterator.
  auto eol_it = (eol_rit == stack_.rend()) ?
        stack_.end() : std::next(eol_rit).base();
  return eol_it;
}

template<typename Key>
auto AssociativeSet<Key>::find_RRIP_Distance(size_t pos) {
  // Helper lambda to find most distant reuse distance.
  auto find_distant = [this, &stack = stack_]() {
    int i = 0;
    for (auto it = stack.begin(); it != stack.end(); ++it) {
      const auto& entry = std::get<Stack_entry>(*it);
      if (entry.RR_distance == decltype(entry.RR_distance)::MIN) {
        replacementReciency_ += i;
        return it;
      }
      ++i;
    }
    return stack.end();
  };

  // Helper lambda to decrement a given reuse distance for a stack entry.
  auto age_distance = [](Stack_value& s) {
    auto& entry = std::get<Stack_entry>(s);
    --entry.RR_distance;
  };

  // Edge cases:
  if (stack_.empty()) {
    return stack_.end();
  }
  if (stack_.size() <= pos) {
    return std::next(stack_.rbegin()).base();
  }

  // RRIP's distance search & counter adjust loop:
  do {
    const auto it = find_distant();
    if (it != stack_.end()) {
      return it;
    }
    std::for_each(stack_.begin(), stack_.end(), age_distance);
  } while (true);
}


// Returns iterator to one past insert position.
template<typename Key>
auto AssociativeSet<Key>::insert_find(Key k) {
  if (stack_.empty()) {
    return stack_.end();
  }
  const Policy& p = p_insert_;
  Stack_it it;
  switch (p.get()) {
  case InsertionPolicy::HP_BYPASS:
    // TODO: defaults to MRU, but maybe insert in another position?
    // - relaltive to confidence of reuse?
  case Policy::MRU:
    it = find_MRU(p.offset());
    break;
  case Policy::LRU:
    it = find_LRU(p.offset());
    break;
  case InsertionPolicy::SHIP:
    it = find_SHiP_Reuse(k, p.offset());
    break;
  case InsertionPolicy::BYPASS:
    it = find_Bypass(k, p.offset());
    break;
  case Policy::RANDOM:
    it = find_Random(p.offset());
    break;
  default:
    throw std::runtime_error("Unknown Insertion Policy.");
  }
  return it;
}


// Returns iterator of victim.
template<typename Key>
auto AssociativeSet<Key>::replace_find() {
  if (stack_.empty()) {
    return stack_.end();
  }
  const Policy& p = p_replace_;
  Stack_it it;
  switch (p.get()) {
  case Policy::LRU:
    it = find_LRU(p.offset());
    replacementLRU_++;
    break;
  case Policy::MRU:
    it = find_MRU(p.offset());
    break;
  case ReplacementPolicy::HP_LRU:
  case ReplacementPolicy::BURST_LRU:
    it = find_Expired(p.offset());
    if (it == stack_.end()) {
      it = find_LRU(p.offset());
      replacementLRU_++;
    }
    else {
      replacementEarly_++;
    }
    break;
  case ReplacementPolicy::SRRIP:
  case ReplacementPolicy::SRRIP_CB:
    it = find_RRIP_Distance(p.offset());
    /* NOTE: implicitly sorts by LRU when multiple elemnts have identical reuse
     * distances.  This may skew results in favor of rrip... */
    break;
  case Policy::RANDOM:
    it = find_Random(p.offset());
    break;
  default:
    throw std::runtime_error("Unknown Replacement Policy.");
  }
  return it;
}


template<typename Key>
void AssociativeSet<Key>::set_insert_policy(Policy insert) {
  p_insert_ = insert;
}

template<typename Key>
void AssociativeSet<Key>::set_replacement_policy(Policy replace) {
  p_replace_ = replace;
}

template<typename Key>
bool AssociativeSet<Key>::operator==(const AssociativeSet<Key>& other) {
  auto compare = [](const Stack_value& a, const Stack_value& b) {
    return a.first == b.first;
  };
  return std::equal(stack_.begin(), stack_.end(), other.stack_.begin(), compare);
}

template<typename Key>
bool AssociativeSet<Key>::operator==(const Stack& other) {
  auto compare = [](const Stack_value& a, const Stack_value& b) {
    return a.first == b.first;
  };
  return std::equal(stack_.begin(), stack_.end(), other.begin(), compare);
}


///////////////////////////////
/// CacheSim Implementation ///
///////////////////////////////
template<typename Key>
CacheSim<Key>::CacheSim(size_t entries, size_t ways) :
  MAX_(entries),  hp_(64*1024), fa_ref_(entries, hp_) {
  // Sanity checks:
  if (ways > 0) {
    if (entries%ways != 0) {
      throw std::runtime_error("Cache associativity need to be power of 2.");
    }
    sets_ = std::vector<AssociativeSet<Key>>(entries/ways, AssociativeSet<Key>(ways, hp_));
  }
}


// Key has been created (first occurance)
template<typename Key>
auto CacheSim<Key>::insert(const Key& k, const Time& t, Features f) {
  auto ref_victim = fa_ref_.insert(k, t, f);
//  test_ref_.insert(k, t);
//  assert(fa_ref_ == test_ref_.get_stack());

  // Determine index->set mapping:
  if (!sets_.empty()) {
    std::size_t hash = std::hash<Key>{}(k);
    hash %= sets_.size();
    auto way_victim = sets_[hash].insert(k, t, f);
    return way_victim;
  }
  return ref_victim;
}


// Key has been accessed again (not first occurance)
// - Determine if hit or capacity conflict.
template<typename Key>
auto CacheSim<Key>::update(const Key& k, const Time& t, Features f) {
  auto ref_victim = fa_ref_.update(k, t, f);
//  test_ref_.update(k, t);
//  assert(fa_ref_.get_stack() == test_ref_.get_stack());

  // Determine index->set mapping:
  if (!sets_.empty()) {
    std::size_t hash = std::hash<Key>{}(k);
    hash %= sets_.size();
    auto way_victim = sets_[hash].update(k, t, f);
    // sanity check (true=hit, false=miss):
    if (ref_victim.first && !way_victim.first)
      conflictMiss_++;
    else if (!ref_victim.first && !way_victim.first)
      capacityMiss_++;
    else if (!ref_victim.first && way_victim.first)
      conflictHit_++;

    return way_victim;
  }
  return ref_victim;
}


// Flush entry (indexed by key):
template<typename Key>
auto CacheSim<Key>::flush(const Key& k) {
  auto ref_victim = fa_ref_.flush(k);

  // Determine index->set mapping:
  if (!sets_.empty()) {
    std::size_t hash = std::hash<Key>{}(k);
    hash %= sets_.size();
    auto way_victim = sets_[hash].flush(k);
    return way_victim;
  }
  return ref_victim;
}

template<typename Key>
auto CacheSim<Key>::flush() {
  using Stack = typename AssociativeSet<Key>::Stack;
  Stack elements;
  for (auto& s : sets_) {
    elements.insert(elements.end(), sets_.flush());
  }
  if (sets_.empty()) {
    elements.insert(elements.end(), fa_ref_.flush());
  }
  return elements;
}


// Key has been invalidated (never to be seen again)
// Even if touched within Epoc, mark invalidated.
// - Evict: Mark 0 at (k, t-1)??
//template<typename Key>
//bool SimLRU<Key>::demote(const Key& k) {
//  // TODO: Consider demoting to a lower position, but not end-of-stack?
//  auto status = lookup_.find(k);
//  if (status != lookup_.end()) {
////    hits_++;
//    stack_it it = status->second;
//    if (it != stack_.begin()) {
//      // Move element from current position to end of list (LSP):
//      stack_.splice(stack_.end(), stack_, it, std::next(it));
//    }
//    return true;
//  }
//  return false;
//}

template<typename Key>
void CacheSim<Key>::set_insert_policy(Policy insert) {
//  fa_ref_.set_insert_policy(insert);
  for (auto& set : sets_) {
    set.set_insert_policy(insert);
  }
}

template<typename Key>
void CacheSim<Key>::set_replacement_policy(Policy replace) {
//  fa_ref_.set_replacement_policy(replace);
  for (auto& set : sets_) {
    set.set_replacement_policy(replace);
  }
}


// CacheSim stats calculations:
template<typename Key>
int64_t CacheSim<Key>::get_fa_hits() const {
  return fa_ref_.get_hits();
}

template<typename Key>
int64_t CacheSim<Key>::get_fa_capacity_miss() const {
  return fa_ref_.get_capacity_miss();
}

template<typename Key>
int64_t CacheSim<Key>::get_hits() const {
  int64_t sum = 0;
  for (const auto& set : sets_) {
    sum += set.get_hits();
  }
  return sum;
}

template<typename Key>
int64_t CacheSim<Key>::get_compulsory_miss() const {
  int64_t sum = 0;
  for (const auto& set : sets_) {
    sum += set.get_compulsory_miss();
  }
  assert(sum == fa_ref_.get_compulsory_miss());
  return fa_ref_.get_compulsory_miss();
}

template<typename Key>
int64_t CacheSim<Key>::get_pure_capacity_miss() const {
  return capacityMiss_;
}

template<typename Key>
int64_t CacheSim<Key>::get_pure_conflict_misses() const {
  return conflictMiss_;
}

template<typename Key>
std::tuple<int64_t,int64_t,int64_t> CacheSim<Key>::get_misses() const {
  int64_t compulsory = 0, capacity = 0;
  for (const auto& set : sets_) {
    compulsory += set.get_compulsory_miss();
    capacity += set.get_capacity_miss();
  }
  int64_t conflict = capacity - fa_ref_.get_capacity_miss();
  capacity -= conflict;

  assert(compulsory == fa_ref_.get_compulsory_miss());
  assert(conflict == conflictMiss_ - conflictHit_);
  assert(capacity == capacityMiss_ + conflictHit_);
  return std::make_tuple(compulsory, capacity, conflict);
}

template<typename Key>
int64_t CacheSim<Key>::get_pure_conflict_hits() const {
  return conflictHit_;
}

template<typename Key>
int64_t CacheSim<Key>::get_replacements_lru() const {
  int64_t sum = 0;
  for (const auto& set : sets_) {
    sum += set.get_replacement_lru();
  }
  return sum;
}

template<typename Key>
int64_t CacheSim<Key>::get_replacements_early() const {
  int64_t sum = 0;
  for (const auto& set : sets_) {
    sum += set.get_replacement_early();
  }
  return sum;
}

template<typename Key>
int64_t CacheSim<Key>::get_prediction_bc() const {
  int64_t sum = 0;
  for (const auto& set : sets_) {
    sum += set.get_prediction_bc();
  }
  return sum;
}

template<typename Key>
int64_t CacheSim<Key>::get_prediction_rc() const {
  int64_t sum = 0;
  for (const auto& set : sets_) {
    sum += set.get_prediction_rc();
  }
  return sum;
}

template<typename Key>
int64_t CacheSim<Key>::get_prediction_rc_better() const {
  int64_t sum = 0;
  for (const auto& set : sets_) {
    sum += set.get_prediction_rc_better();
  }
  return sum;
}

template<typename Key>
int64_t CacheSim<Key>::get_replacements_eager() const {
  int64_t sum = 0;
  for (const auto& set : sets_) {
    sum += set.get_prediction_too_early();
  }
  return sum;
}

template<typename Key>
int64_t CacheSim<Key>::get_insert_predict_distant() const {
  int64_t sum = 0;
  for (const auto& set : sets_) {
    sum += set.get_insert_predict_distant();
  }
  return sum;
}

template<typename Key>
double CacheSim<Key>::get_replacement_reciency() const {
  int64_t sum = 0, misses = 0;
  for (const auto& set : sets_) {
    sum += set.get_insert_predict_distant();
    misses += set.get_compulsory_miss() + set.get_capacity_miss();
  }
  return double(sum) / double(misses);
}

template<typename Key>
std::pair<int64_t, int64_t> CacheSim<Key>::get_prediction_hp() const {
  std::pair<int64_t,int64_t> predictions = {};
  for (const auto& set : sets_) {
    auto p = set.get_prediction_hp();
    predictions.first += p.first;
    predictions.second += p.second;
  }
  return predictions; // {bypass, evict}
}

template<typename Key>
std::string CacheSim<Key>::print_stats() {
  std::stringstream ss;
  ss << "SimCache Size: " << get_size() << '\n';
  ss << " - Associativity: " << get_associativity() << "-way\n";
  ss << " - Sets: " << num_sets() << '\n';
  ss << " - Hits: " << get_hits() << '\n';
  {
    auto [compulsory, capacity, conflict] = get_misses();
    ss << " - Miss (Compulsory): " << compulsory << '\n';
    ss << " - Miss (Capacity): " << capacity << '\n';
    ss << " - Miss (Conflict): " << conflict << '\n';
  }
  ss << " - Hits FA: " << get_fa_hits() << '\n';
  ss << " - Miss FA (Capacity): " << get_fa_capacity_miss() << '\n';
  ss << " - Pure Conflict Hits: " << get_pure_conflict_hits() << '\n';
  ss << " - Pure Conflict Misses: " << get_pure_conflict_misses() << '\n';
  ss << " - Replacements LRU: " << get_replacements_lru() << '\n';
  ss << " - Replacements Early: " << get_replacements_early() << '\n';
  ss << " - Confident BurstCount: " << get_prediction_bc() << '\n';
  ss << " - Confident RefCount: " << get_prediction_rc() << '\n';
  ss << " - RefCount tops BurstCount: " << get_prediction_rc_better() << '\n';
  ss << " - Eager Replace Caught: " << get_replacements_eager() << '\n';
  ss << " - Insert Predict Distant: " << get_insert_predict_distant() << '\n';
  ss << " - Replacement Reciency: " << get_replacement_reciency() << '\n';
  // TODO:
  // print incorrect prediction
  // dont penalize on prediction too much, bound confidence for flexibility?
  // way to leverage min for replacement insight?
  ss << hp_.hp_.print_stats();
  return ss.str();
}

template<typename Key>
hpHandle<Key>& CacheSim<Key>::get_hp_handle() {
  return hp_;
}

#endif // SIM_CACHE_HPP
