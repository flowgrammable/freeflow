#ifndef SIM_LRU_HPP
#define SIM_LRU_HPP

#include <vector>
#include <iostream>

#include "types.hpp"
#include "util_container.hpp"

// Google's Abseil C++ library:
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"


template<typename Key>
class SimLRU {
  using Count = uint32_t;
  using Time = timespec;  // maybe throw out?
//  using Point = std::pair<size_t,Time>;  // miss-index, real-time
//  using Reservation = std::pair<Point,Point>;
  using Reservation = std::pair<size_t,size_t>;
//  using History = absl::InlinedVector<Reservation,1>;

public:
  SimLRU(size_t entries);
  void insert(const Key& k, const Time& t);
  bool update(const Key& k, const Time& t);
  bool demote(const Key& k);

  size_t get_size() const {return MAX_;}
  uint64_t get_hits() const {return hits_;}
  uint64_t get_capacity_miss() const {return capacityMiss_;}
  uint64_t get_compulsory_miss() const {return compulsoryMiss_;}

  const auto& get_stack() const {return stack_;}

private:
  auto internal_insert(const Key& k, const Time& t);

private:
  using lru_stack_t = std::list< std::pair<Key, Reservation> >;
  using lru_stack_it = typename lru_stack_t::iterator;

  const size_t MAX_;
  lru_stack_t stack_;
  absl::flat_hash_map<Key, lru_stack_it> lookup_;

  // Stats (summary from columns in MIN table):
  uint64_t hits_ = 0;
  uint64_t capacityMiss_ = 0;
  uint64_t compulsoryMiss_ = 0;
//  uint64_t invalidates_ = 0;
};


/////////////////////////////
/// SimMin Implementation ///
/////////////////////////////

template<typename Key>
SimLRU<Key>::SimLRU(size_t entries) : MAX_(entries) {
  lookup_.reserve(entries+1);
}


// Internal function to manage insertion into LRU stack.
// - Returns evicted std::pair<Key, Reservation> if stack >= MAX.
// - Returns std::pair<Key(0), Reservation()> if < MAX
template<typename Key>
auto SimLRU<Key>::internal_insert(const Key& k, const Time& t) {
  size_t column = compulsoryMiss_ + capacityMiss_;  // t
  {
    Reservation res = std::make_pair(column,column);
    auto item = std::make_pair(k, res);
    stack_.push_front( item );
  }
  lookup_.insert( std::make_pair(k, stack_.begin()) );
  assert(lookup_.size() == stack_.size());
  if (stack_.size() > MAX_) {
    auto evicted = std::move(stack_.back());
    Key k = evicted.first;
    lookup_.erase(k);
    stack_.pop_back();
    return evicted;
  }
  return std::make_pair(Key(0), Reservation());
}


// Key has been created (first occurance)
template<typename Key>
void SimLRU<Key>::insert(const Key& k, const Time& t) {
  compulsoryMiss_++;
  internal_insert(k, t);
}


// Key has been accessed again (not first occurance)
// - Determine if hit or capacity conflict.
template<typename Key>
bool SimLRU<Key>::update(const Key& k, const Time& t) {
  auto status = lookup_.find(k);
  if (status != lookup_.end()) {
    hits_++;
    lru_stack_it it = status->second;
    if (it != stack_.begin()) {
      // Move element from current position to front of list (MSP):
      stack_.splice(stack_.begin(), stack_, it, std::next(it));
    }
    return true;  // hit
  }
  else {
    capacityMiss_++;
    auto evicted = internal_insert(k, t);
    return false; // miss
  }
}


// Key has been invalidated (never to be seen again)
// Even if touched within Epoc, mark invalidated.
// - Evict: Mark 0 at (k, t-1)??
template<typename Key>
bool SimLRU<Key>::demote(const Key& k) {
  // TODO: Consider demoting to a lower position, but not end-of-stack?
  auto status = lookup_.find(k);
  if (status != lookup_.end()) {
//    hits_++;
    lru_stack_it it = status->second;
    if (it != stack_.begin()) {
      // Move element from current position to end of list (LSP):
      stack_.splice(stack_.end(), stack_, it, std::next(it));
    }
    return true;
  }
  return false;
}


#endif // SIM_LRU_HPP
