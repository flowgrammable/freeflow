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

#include "types.hpp"
#include "util_container.hpp"
//#include "sim_lru.hpp"
//#include "sim_min.hpp"

// Google's Abseil C++ library:
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"

//template<typename InputIt>
//InputIt find_LRU(InputIt first, InputIt last, size_t pos) {
//  auto r_first = std::make_reverse_iterator(last);
//  auto r_last = std::make_reverse_iterator(first);
//  auto r_it = r_first;
//  for (size_t i = 0; i < pos; i++) {
//    auto next = std::next(r_it);
//    if (next == r_last) {
//      return std::next(r_it).base();
//    }
//    r_it = next;
//  }
//  return std::next(r_it).base();
//}

//template<typename InputIt>
//InputIt find_MRU(InputIt first, InputIt last, size_t pos) {
//  auto it = first;
//  for (size_t i = 0; i < pos; i++) {
//    auto next = std::next(it);
//    if (next == last) {
//      return it;
//    }
//    it = next;
//  }
//  return it;
//}

//// Requires InputIt to be reverse iterable and ordered from MRU to LRU:
//template<typename InputIt>
//class Policy {
//public:
//  virtual InputIt find(InputIt first, InputIt last) = 0;
//};

//template<typename InputIt>
//class Policy_LRU : public Policy<InputIt> {
//public:
//  Policy_LRU(size_t pos = 0) : pos_(pos) {}
//  InputIt find(InputIt first, InputIt last) {
//    auto r_first = std::make_reverse_iterator(last);
//    auto r_last = std::make_reverse_iterator(first);
//    auto r_it = r_first;
//    for (size_t i = 0; i < pos_; i++) {
//      auto next = std::next(r_it);
//      if (next == r_last) {
//        return std::next(r_it).base();
//      }
//      r_it = next;
//    }
//    return std::next(r_it).base();
////    if (s.size() <= pos_) {
////      return s.rbegin().base();
////    }
////    return (s.rbegin() + pos_).base();
//  }
//private:
//  const size_t pos_;
//};

//template<typename InputIt>
//class Policy_MRU : public Policy<InputIt> {
//public:
//  Policy_MRU(size_t pos = 0) : pos_(pos) {}
//  InputIt find(InputIt first, InputIt last) {
//    auto it = first;
//    for (size_t i = 0; i < pos_; i++) {
//      auto next = std::next(it);
//      if (next == last) {
//        return it;
//      }
//      it = next;
//    }
//    return it;
////    if (s.size() <= pos_) {
////      return s.rbegin().base();
////    }
////    return (s.begin() + pos_).base();
//  }
//private:
//  const size_t pos_;
//};


struct Policy {
  enum PolicyType {LRU, MRU, BURST_LRU};
  Policy(PolicyType type) : type_(type) {}

  PolicyType type_;
  size_t offset_ = 0;
};


// Global type definitions:
using Time = timespec;  // maybe throw out?
using MIN_Time = size_t;
using Reservation = std::pair<MIN_Time, MIN_Time>; // refactor to Time?
using HitStats = std::vector<int>;  // mru burst hit counter


// Defines cache entry:
struct Stack_entry {
  Stack_entry(Reservation r) : res(r) {}

  // Lifetime Prediction Counters:
  int64_t refCount = 1;   // counts hits at MRU position
//  int burstCount = 0; // counts bursts from non-MRU to MRU position
  bool eol = false;   // indicates predicted to be end-of-lifetime

  // Statistics:
  Reservation res;
  HitStats hits = {1};
};

// Defines prediciton table entry:
struct PT_entry {
  int64_t BC_saved = -1;  // BurstCount (-1 indicates 1st insertion)
  int64_t RC_saved = -1;  // RefCount
  ClampedInt<3> BC_confidence = -1; // 3-bit saturating counter.
  ClampedInt<3> RC_confidence = -1;
//  short BC_confidence = -2;  // psudo confience metric (++, match; --, unmatched)
//  short RC_confidence = -2;

  // Statistics:
  HitStats history_hits;
  std::vector<Reservation> history_res;
};


template<typename Key>
class AssociativeSet {
public:
  using Stack = std::list< std::pair<Key, Stack_entry> >;
  using Stack_it = typename Stack::iterator;
  using Stack_value = typename Stack::value_type;
  using Evict_t = std::tuple<Key, Reservation, HitStats>;

  using PatternTable = std::map<Key, PT_entry>;

  AssociativeSet(size_t entries,
                 Policy&& insert = Policy(Policy::MRU),
                 Policy&& replace = Policy(Policy::LRU));

  auto insert(const Key& k, const Time& t);
  auto update(const Key& k, const Time& t);
  bool demote(const Key& k);
  auto flush(const Key& k);
  auto flush();

  auto insert_find();
  auto replace_find();

  void set_insert_policy(Policy insert);
  void set_replacement_policy(Policy replace);

  const auto& get_stack() const {return stack_;}

  // Stats:
  int64_t get_hits() const {return hits_;}
  int64_t get_capacity_miss() const {return capacityMiss_;}
  int64_t get_compulsory_miss() const {return compulsoryMiss_;}
  int64_t get_replacement_lru() const {return replacementLRU_;}
  int64_t get_prediction_bc() const {return predictionBC_;}
  int64_t get_prediction_rc() const {return predictionRC_;}
  int64_t get_prediction_rc_better() const {return predictionRC_better_;}
  int64_t get_prediction_too_early() const {return predictionTooEarly_;}

  // Stack comparison for debugging:
  bool operator==(const AssociativeSet<Key>& other);
  bool operator==(const Stack& other);

private:
  auto internal_insert(const Key& k, const Time& t);
  void event_eviction(Stack_it eviction);
  void event_mru_demotion(Stack_it demoted);

  auto find_MRU(size_t pos);
  auto find_LRU(size_t pos);
  auto find_Expired(size_t pos);

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
  // Predictor Stats
  int64_t replacementLRU_ = 0;
  int64_t predictionBC_ = 0;
  int64_t predictionRC_ = 0;
  int64_t predictionRC_better_ = 0;
  int64_t predictionTooEarly_ = 0;

  // Burst Prediction:
  std::map<Key, PT_entry> pt_;  // Prediction metadata independent of cache.
};


template<typename Key>
class CacheSim {
public:
  using Time = timespec;  // maybe throw out?
  using Stack_value = typename AssociativeSet<Key>::Stack_value;
//  using Reservation = typename AssociativeSet<Key>::Reservation;
//  using HitStats = typename AssociativeSet<Key>::HitStats;

  CacheSim(size_t entries, size_t ways = 0);
  auto insert(const Key& k, const Time& t);
  auto update(const Key& k, const Time& t);
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

  void set_insert_policy(Policy& insert);
  void set_replacement_policy(Policy& insert);

  // Stats:
  int64_t get_hits() const;
  int64_t get_fa_hits() const;
  int64_t get_compulsory_miss() const;
  int64_t get_capacity_miss() const;
  int64_t get_fa_capacity_miss() const;
  int64_t get_conflict_miss() const;
  int64_t get_alias_hits() const;
  int64_t get_replacements_lru() const;
  int64_t get_replacements_early() const;
  int64_t get_prediction_bc() const;
  int64_t get_prediction_rc() const;
  int64_t get_prediction_rc_better() const;
  int64_t get_prediction_too_early() const;
  std::string print_stats() const;

private:
  // Cache Config:
  const size_t MAX_;  // Total cache elements

  // Cache Containers:
  AssociativeSet<Key> fa_ref_;  // Conflict reference: Fully Associative
  std::vector< AssociativeSet<Key> > sets_;  // Vector of associative sets
//  SimLRU<Key> test_ref_;

  // Sanity check stats:
  int64_t capacityMiss_ = 0;
  int64_t conflictMiss_ = 0;
  int64_t aliasHit_ = 0;
};


/////////////////////////////////////
/// AssociativeSet Implementation ///
/////////////////////////////////////
template<typename Key>
AssociativeSet<Key>::AssociativeSet(size_t entries, Policy&& insert, Policy&& replace) :
  MAX_(entries), lookup_(entries), p_insert_(insert), p_replace_(replace) {}

// Key has been created (first occurance)
template<typename Key>
auto AssociativeSet<Key>::insert(const Key& k, const Time& t) {
  compulsoryMiss_++;
  return internal_insert(k, t);
}

// Key has been accessed again (not first occurance)
// - Determine if hit or capacity conflict.
// - Returns {hit/miss, eviction (if necessary)}
template<typename Key>
auto AssociativeSet<Key>::update(const Key& k, const Time& t) {
  MIN_Time column = compulsoryMiss_ + capacityMiss_;  // Comparable to Min's t
  auto status = lookup_.find(k);
  if (status != lookup_.end()) {
    hits_++;
    Stack_it it = status->second;
    Stack_entry& e = std::get<Stack_entry>(*it);
    Reservation& r = e.res;
    HitStats& s = e.hits;
    e.refCount++;
    r.second = column;
    // TODO: factor out into promotion function if modularity is needed:
    if (it == stack_.begin()) {
      s.back()++; // at MRU (continuting burst)
    }
    else {
      // Move element from current position to front of list (MSP):
//      s.insert(s.end(), 1); // moving up to MRU (new burst)
      s.emplace_back(1);  // new burst
      Stack_it demotion = stack_.begin(); // let naturally demote by 1 position
      stack_.splice(stack_.begin(), stack_, it, std::next(it));
      event_mru_demotion(demotion);
    }
    return std::make_pair(true, std::optional<Evict_t>{});  // hit
  }
  else {
    capacityMiss_++;
    auto evicted = internal_insert(k, t);
    return std::make_pair(false, evicted);  // miss
  }
}

// Internal function to manage insertion into a generic reciency stack.
// - Returns evicted std::pair<Key, Reservation> if stack >= MAX.
// - Returns default constructed std::pair<Key(0), Reservation()> if < MAX
template<typename Key>
auto AssociativeSet<Key>::internal_insert(const Key& k, const Time& t) {
  MIN_Time column = compulsoryMiss_ + capacityMiss_;  // Comparable to Min's t
  Stack_entry new_e = Stack_entry(Reservation(column,column));

  // Replacement Policy:
  std::optional<Evict_t> eviction;
  if (stack_.size() >= MAX_) {
    auto victim_it = replace_find();
    Key key = std::get<Key>(*victim_it);
    Stack_entry& victim_e = std::get<Stack_entry>(*victim_it);

    // Note: calling event_eviction before evicting to preserve stack...
    event_eviction(victim_it);
    lookup_.erase(key);

    eviction = Evict_t(key, std::move(victim_e.res), std::move(victim_e.hits));
    stack_.erase(victim_it);
  }
  // else evicted (std::optional) is left default constructed.

  // Insert Policy:
  auto insert_it = insert_find();
  auto item_it = stack_.emplace(insert_it, Stack_value(k, new_e));
  lookup_.insert( std::make_pair(k,item_it) );

  assert(lookup_.size() == stack_.size());
  return eviction;   // default constructed
}


// Flushes entry (indexed by Key):
template<typename Key>
auto AssociativeSet<Key>::flush(const Key& k) {
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
  const HitStats& hitStats = entry.hits;
  const Reservation& res = entry.res;

  // Calculate burst & reference count:
  const int64_t burstCount = hitStats.size();
//  int refCount = std::accumulate(hitStats.begin(), hitStats.end(), 0);
  const auto refCount = entry.refCount;

  // Create/Update Pattern Table entry.  (Note: only PT creation event)
  PT_entry& pte = pt_[key];

  // Update BurstCount pattern table:
  if (burstCount == pte.BC_saved) {
    ++pte.BC_confidence;
  }
  else {
    pte.BC_saved = burstCount;  // update to last burst count.
    --pte.BC_confidence;
  }

  // Update ReferenceCount pattern table:
  if (refCount == pte.RC_saved) {
    ++pte.RC_confidence;
  }
  // TODO: else if rc is in tolerable delta, don't penalize?
  else {
    pte.RC_saved = refCount;
    --pte.RC_confidence;
  }

  // Update statistics:
//  auto MINLifetime = res.second - res.first;  // how many misses did this element survive?
  pte.history_hits.insert(pte.history_hits.end(), hitStats.begin(), hitStats.end());
  pte.history_res.push_back(res);
}


// Reciency stack change event triggers this function to note metadata as needed.
// - Called post stack adjustment with iterator pointing to demoted entry.
template<typename Key>
void AssociativeSet<Key>::event_mru_demotion(Stack_it demoted) {
  { // Take notes on demoted MRU:
    auto& [key, entry] = *demoted;
    const HitStats& hitStats = entry.hits;

    const int64_t burstCount = hitStats.size();
//    int refCount = std::accumulate(hitStats.begin(), hitStats.end(), 0);
    const auto refCount = entry.refCount;
    PT_entry& pte = pt_[key];

    assert(!entry.eol); // sanity check
    if (pte.BC_confidence >= 0 && burstCount >= pte.BC_saved) {
      predictionBC_++;
      entry.eol = true;   // mark as candidate for replacement
    }
    if (pte.RC_confidence >= 0 && refCount >= pte.RC_saved) {
      if (!entry.eol) {   // hack to count if RC > BC
        predictionRC_better_++;
      }
      predictionRC_++;
      entry.eol = true;   // mark as candidate for replacement
    }
  }

  { // Take notes on new MRU:
    Stack_it mru = stack_.begin();
    auto& [key, entry] = *mru;
    entry.eol = false;  // ensure not marked for replacement
    predictionTooEarly_++;
  }
}


template<typename Key>
auto AssociativeSet<Key>::find_MRU(size_t pos) {
  if (stack_.empty()) {
    return stack_.end();
  }
  if (stack_.size() <= pos) {
    return std::next( stack_.rbegin() ).base(); // untested
  }
  return std::next(stack_.begin(), pos);
}


template<typename Key>
auto AssociativeSet<Key>::find_LRU(size_t pos) {
  if (stack_.empty()) {
    return stack_.end();
  }
  if (stack_.size() <= pos) {
    return std::next( stack_.rbegin() ).base(); // untested
  }
  return std::next(stack_.rbegin(), pos+1).base();
}


template<typename Key>
auto AssociativeSet<Key>::find_Expired(size_t pos) {
  auto lifetime_expired = [](const Stack_value& s) {
    return std::get<Stack_entry>(s).eol;
  };

  // Find first (oldest referenced) replacement candidate:
  // TODO: explore oldest {detected, stack position, confidence}
  auto eol_rit = std::find_if(stack_.rbegin(), stack_.rend(), lifetime_expired);
  // Convert to forward iterator.
  auto eol_it = (eol_rit == stack_.rend()) ?
        stack_.end() : std::next(eol_rit).base();
  return eol_it;
}


// Returns iterator to one past insert position.
template<typename Key>
auto AssociativeSet<Key>::insert_find() {
  if (stack_.empty()) {
    return stack_.end();
  }
  Stack_it it;
  switch (p_insert_.type_) {
  case Policy::LRU:
    it = find_LRU(p_insert_.offset_);
    break;
  case Policy::MRU:
    it = find_MRU(p_insert_.offset_);
    break;
  default:
    throw std::runtime_error("Unknown Insert Policy.");
  }
  return it;
}


// Returns iterator of victim.
template<typename Key>
auto AssociativeSet<Key>::replace_find() {
  if (stack_.empty()) {
    return stack_.end();
  }
  Stack_it it;
  switch (p_replace_.type_) {
  case Policy::LRU:
    it = find_LRU(p_replace_.offset_);
    replacementLRU_++;
    break;
  case Policy::MRU:
    it = find_MRU(p_replace_.offset_);
    break;
  case Policy::BURST_LRU:
    it = find_Expired(p_replace_.offset_);
    if (it == stack_.end()) {
      it = find_LRU(p_replace_.offset_);
      replacementLRU_++;
    }
    break;
  default:
    throw std::runtime_error("Unknown Replace Policy.");
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
  MAX_(entries), fa_ref_(AssociativeSet<Key>(entries))/*, test_ref_(entries)*/ {
  // Sanity checks:
  if (ways > 0) {
    if (entries%ways != 0) {
      throw std::runtime_error("Cache associativity need to be power of 2.");
    }
    sets_ = std::vector<AssociativeSet<Key>>(entries/ways, AssociativeSet<Key>(ways));
  }
}


// Key has been created (first occurance)
template<typename Key>
auto CacheSim<Key>::insert(const Key& k, const Time& t) {
  auto ref_victim = fa_ref_.insert(k, t);
//  test_ref_.insert(k, t);
//  assert(fa_ref_ == test_ref_.get_stack());

  // Determine index->set mapping:
  if (!sets_.empty()) {
    std::size_t hash = std::hash<Key>{}(k);
    hash %= sets_.size();
    auto way_victim = sets_[hash].insert(k, t);
    return way_victim;
  }
  return ref_victim;
}


// Key has been accessed again (not first occurance)
// - Determine if hit or capacity conflict.
template<typename Key>
auto CacheSim<Key>::update(const Key& k, const Time& t) {
  auto ref_victim = fa_ref_.update(k, t);
//  test_ref_.update(k, t);
//  assert(fa_ref_.get_stack() == test_ref_.get_stack());

  // Determine index->set mapping:
  if (!sets_.empty()) {
    std::size_t hash = std::hash<Key>{}(k);
    hash %= sets_.size();
    auto way_victim = sets_[hash].update(k, t);
    // sanity check (true=hit, false=miss):
    if (ref_victim.first && !way_victim.first)
      conflictMiss_++;
    else if (!ref_victim.first && !way_victim.first)
      capacityMiss_++;
    else if (!ref_victim.first && way_victim.first)
      aliasHit_++;

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
void CacheSim<Key>::set_insert_policy(Policy& insert) {
//  fa_ref_.set_insert_policy(insert);
  for (auto& set : sets_) {
    set.set_insert_policy(insert);
  }
}

template<typename Key>
void CacheSim<Key>::set_replacement_policy(Policy& replace) {
//  fa_ref_.set_replacement_policy(replace);
  for (auto& set : sets_) {
    set.set_replacement_policy(replace);
  }
}


// CacheSim stats calculations:
template<typename Key>
int64_t CacheSim<Key>::get_hits() const {
//  std::cout << "FA Hits: " << fa_ref_.get_hits() << std::endl;
  int64_t hits = 0;
  for (const auto& set : sets_) {
    hits += set.get_hits();
  }
//  std::cout << "SA Hits: " << capacityMiss_ << std::endl;
  return hits;
}

template<typename Key>
int64_t CacheSim<Key>::get_fa_hits() const {
  return fa_ref_.get_hits();
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
int64_t CacheSim<Key>::get_capacity_miss() const {
//  std::cout << "FA Capacity Miss: " << fa_ref_.get_capacity_miss() << std::endl;
//  std::cout << "SA Capacity Miss: " << capacityMiss_ << std::endl;
  return capacityMiss_;
}

template<typename Key>
int64_t CacheSim<Key>::get_fa_capacity_miss() const {
  return fa_ref_.get_capacity_miss();
}

template<typename Key>
int64_t CacheSim<Key>::get_conflict_miss() const {
  // SUM(set_[].misses()) - FA.misses()
  int64_t conflict_sum = -fa_ref_.get_capacity_miss();
  for (const auto& set : sets_) {
    conflict_sum += set.get_capacity_miss();
  }
//  std::cout << "Calculated Conflict Miss: " << conflict_sum << std::endl;
//  std::cout << "Incremental Conflict Miss: " << conflictMiss_ << std::endl;
//  std::cout << "Alias Conflict Hit: " << aliasHit_ << std::endl;
  return conflictMiss_;
}

template<typename Key>
int64_t CacheSim<Key>::get_alias_hits() const {
  return aliasHit_;
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
  int64_t replacements_lru = 0;
  int64_t misses = 0;
  for (const auto& set : sets_) {
    replacements_lru += set.get_replacement_lru();
    misses += set.get_compulsory_miss() + set.get_capacity_miss();
  }
  return misses - replacements_lru;
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
int64_t CacheSim<Key>::get_prediction_too_early() const {
  int64_t sum = 0;
  for (const auto& set : sets_) {
    sum += set.get_prediction_too_early();
  }
  return sum;
}

template<typename Key>
std::string CacheSim<Key>::print_stats() const {
  std::stringstream ss;
  ss << "SimCache Size: " << get_size() << '\n';
  ss << " - Associativity: " << get_associativity() << "-way\n";
  ss << " - Sets: " << num_sets() << '\n';
  ss << " - Hits: " << get_hits() << '\n';
  ss << " - Miss (Compulsory): " << get_compulsory_miss() << '\n';
  ss << " - Miss (Capacity): " << get_capacity_miss() << '\n';
  ss << " - Miss (Conflict): " << get_conflict_miss() << '\n';
  ss << " - Hits FA: " << get_fa_hits() << '\n';
  ss << " - Miss FA (Capacity): " << get_fa_capacity_miss() << '\n';
  ss << " - Hits (SA over FA): " << get_alias_hits() << '\n';
  ss << " - Replacements LRU: " << get_replacements_lru() << '\n';
  ss << " - Replacements Early: " << get_replacements_early() << '\n';
  ss << " - Confident BurstCount: " << get_prediction_bc() << '\n';
  ss << " - Confident RefCount: " << get_prediction_rc() << '\n';
  ss << " - RefCount tops BurstCount: " << get_prediction_rc_better() << '\n';
  ss << " - Prediction too Early: " << get_prediction_too_early();
  // TODO:
  // print incorrect prediction
  // dont penalize on prediction too much, bound confidence for flexibility?
  // way to leverage min for replacement insight?
  return ss.str();
}

#endif // SIM_CACHE_HPP
