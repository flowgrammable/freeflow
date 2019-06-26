#ifndef CACHE_SIM_HPP
#define CACHE_SIM_HPP

#include <vector>
#include <list>
#include <iostream>
#include <iterator>

#include "types.hpp"
#include "util_container.hpp"
//#include "sim_lru.hpp"

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
  enum PolicyType {LRU, MRU};
  Policy(PolicyType type) : type_(type) {}

  PolicyType type_;
  size_t offset_ = 0;
};


template<typename Key>
class AssociativeSet {
public:
  using Time = timespec;  // maybe throw out?
  using MIN_Time = size_t;
  using Reservation = std::pair<MIN_Time,MIN_Time>; // refactor to Time?

  using Stack = std::list< std::pair<Key,Reservation> >;
  using Stack_it = typename Stack::iterator;
  using Stack_value = typename Stack::value_type;
//  using Policy_ptr = std::unique_ptr<Policy<Stack_it>>;

  AssociativeSet(size_t entries,
                 Policy&& insert = Policy(Policy::MRU),
                 Policy&& replace = Policy(Policy::LRU));

  auto insert(const Key& k, const Time& t);
  auto update(const Key& k, const Time& t);
  bool demote(const Key& k);

  auto insert_find();
  auto replace_find();

  void set_insert_policy(Policy&& insert);
  void set_replacement_policy(Policy&& replace);

  const auto& get_stack() const {return stack_;}

  // Stats:
  uint64_t get_hits() const {return hits_;}
  uint64_t get_capacity_miss() const {return capacityMiss_;}
  uint64_t get_compulsory_miss() const {return compulsoryMiss_;}

  // Stack comparison for debugging:
  bool operator==(const AssociativeSet<Key>& other);
  bool operator==(const Stack& other);

private:
  auto internal_insert(const Key& k, const Time& t);
  auto find_MRU(size_t pos);
  auto find_LRU(size_t pos);

  // Way Config:
  const size_t MAX_;
  Policy p_insert_;
  Policy p_replace_;

  // Way Containers:
  Stack stack_;
  absl::flat_hash_map<Key,Stack_it> lookup_;

  // Stats:
  uint64_t hits_ = 0;
  uint64_t compulsoryMiss_ = 0;
  uint64_t capacityMiss_ = 0;
};


template<typename Key>
class CacheSim {
public:
//  using Count = uint32_t;
  using Time = timespec;  // maybe throw out?
//  using Point = std::pair<size_t,Time>;  // miss-index, real-time
//  using Reservation = std::pair<Point,Point>;
//  using Reservation = std::pair<size_t,size_t>;
//  using History = absl::InlinedVector<Reservation,1>;

  CacheSim(size_t entries, size_t ways = 0);
  auto insert(const Key& k, const Time& t);
  auto update(const Key& k, const Time& t);
  bool demote(const Key& k);

  size_t get_size() const {return MAX_;}
  AssociativeSet<Key>& get_set(size_t i) const {return sets_.at(i);}

  void set_insert_policy(Policy& insert);
  void set_replacement_policy(Policy& insert);

  // Stats:
  uint64_t get_hits() const {return fa_ref_.get_hits();}
  uint64_t get_compulsory_miss() const {return fa_ref_.get_compulsory_miss();}
  uint64_t get_capacity_miss() const {return fa_ref_.get_capacity_miss();}
  uint64_t get_conflict_miss() const {
    // SUM(set_[].misses()) - FA.misses()
    uint64_t conflict_sum = -fa_ref_.get_capacity_miss();
    for (const auto& set : sets_) {
      conflict_sum += set.get_capacity_miss();
    }
    return conflict_sum;
  }

private:
  // Cache Config:
  const size_t MAX_;  // Total cache elements

  // Cache Containers:
  AssociativeSet<Key> fa_ref_;  // Conflict reference: Fully Associative
  std::vector< AssociativeSet<Key> > sets_;  // Vector of associative sets (N-way)
//  SimLRU<Key> test_ref_;
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
template<typename Key>
auto AssociativeSet<Key>::update(const Key& k, const Time& t) {
  MIN_Time column = compulsoryMiss_ + capacityMiss_;  // Comparable to Min's t
  auto status = lookup_.find(k);
  if (status != lookup_.end()) {
    hits_++;
    Stack_it it = status->second;
    Reservation& r = it->second;
    r.second = column;
    if (it != stack_.begin()) {
      // Move element from current position to front of list (MSP):
      stack_.splice(stack_.begin(), stack_, it, std::next(it));
    }
    return std::make_pair(true, std::optional<Stack_value>{});  // hit
  }
  else {
    capacityMiss_++;
    auto evicted = internal_insert(k, t);
    return std::make_pair(false, std::optional<Stack_value>(evicted));  // miss
  }
}

// Internal function to manage insertion into a generic reciency stack.
// - Returns evicted std::pair<Key, Reservation> if stack >= MAX.
// - Returns default constructed std::pair<Key(0), Reservation()> if < MAX
template<typename Key>
auto AssociativeSet<Key>::internal_insert(const Key& k, const Time& t) {
  MIN_Time column = compulsoryMiss_ + capacityMiss_;  // Comparable to Min's t
  Reservation res = std::make_pair(column,column);
  auto item = std::make_pair(k,res);

  // Replacement Policy:
  Stack_value evicted;
  if (stack_.size() >= MAX_) {
    auto victim_it = replace_find();
    evicted = std::move(*victim_it);
    stack_.erase(victim_it);
    lookup_.erase(evicted.first);
  }
  else {
    evicted = std::make_pair(Key(0), Reservation());
  }

  // Insert Policy:
  auto insert_it = insert_find();
  auto item_it = stack_.emplace(insert_it, item);
  lookup_.insert( std::make_pair(k,item_it) );

  assert(lookup_.size() == stack_.size());
  return evicted;
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
    assert(false);
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
    break;
  case Policy::MRU:
    it = find_MRU(p_replace_.offset_);
    break;
  default:
    assert(false);
  }
  return it;
}


template<typename Key>
void AssociativeSet<Key>::set_insert_policy(Policy&& insert) {
  p_insert_ = insert;
}
template<typename Key>
void AssociativeSet<Key>::set_replacement_policy(Policy&& replace) {
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
  if (ways != 0) {
    assert(entries%ways == 0);
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
  if (sets_.size() != 0) {
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
  if (sets_.size() != 0) {
    std::size_t hash = std::hash<Key>{}(k);
    hash %= sets_.size();
    auto way_victim = sets_[hash].update(k, t);
    return way_victim;
  }
  return ref_victim;
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
  for (auto& set : sets_) {
    set.set_insert_policy(insert);
  }
}
template<typename Key>
void CacheSim<Key>::set_replacement_policy(Policy& replace) {
  for (auto& set : sets_) {
    set.set_insert_policy(replace);
  }
}

#endif // SIM_CACHE_HPP
