#ifndef SIM_MIN_HPP
#define SIM_MIN_HPP

#include <vector>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <type_traits>

#include "types.hpp"
#include "util_container.hpp"

// Google's Abseil C++ library:
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"


template<typename Key>
class SimMIN {
  using Count = uint32_t;
  using Time = timespec;  // maybe throw out?
//  using Point = std::pair<size_t,Time>;  // miss-index, real-time
//  using Reservation = std::pair<Point,Point>;
  using Reservation = std::pair<size_t,size_t>;
  using History = absl::InlinedVector<Reservation,1>;

public:
  SimMIN(size_t entries);

private:
  std::vector<Key> trim_to_barrier();

public:
  void insert(const Key& k, const Time& t);
  auto update(const Key& k, const Time& t);
  void remove(const Key& k);  // does nothing...

  size_t get_size() const {return MAX_;}
  uint64_t get_hits() const {return hits_;}
  uint64_t get_capacity_miss() const {return capacityMiss_;}
  uint64_t get_compulsory_miss() const {return compulsoryMiss_;}
  size_t get_max_elements() const {return max_elements_;}
  auto get_barrier_duration() const {return barrier_duration_;}

private:
  const size_t MAX_;
  size_t barrier_ = 0;      // absolute indexing (adjust with trim_offset)
  size_t trim_offset_ = 0;  // offset for relative indexing
  absl::flat_hash_map<Key, History> reserved_;
  std::vector<Count> capacity_;  // count within columns in MIN table.

  // Stats (summary from columns in MIN table):
  uint64_t hits_ = 0;
  uint64_t capacityMiss_ = 0;
  uint64_t compulsoryMiss_ = 0;
//  uint64_t invalidates_ = 0;
  size_t max_elements_ = 0;
  std::vector<size_t> barrier_duration_;  // number of hits when barrier was moved.
};


/////////////////////////////
/// SimMin Implementation ///
/////////////////////////////

template<typename Key>
SimMIN<Key>::SimMIN(size_t entries) : MAX_(entries) {
  reserved_.reserve(entries*16);  // prepare for roughly 10:1 flow:active ratio
}


// Key has been created (first occurance)
// - Pull:  Mark 1 at (k, t).
template<typename Key>
void SimMIN<Key>::insert(const Key& k, const Time& t) {
  size_t column = compulsoryMiss_ + capacityMiss_;  // t
  compulsoryMiss_++;

  History& hist = reserved_[k];
  hist.emplace_back( std::make_pair(column,column) );
  capacity_.emplace_back(1);
  assert(capacity_.size() == compulsoryMiss_ + capacityMiss_ - trim_offset_);
}

template <class T>
__attribute__((always_inline)) inline void DoNotOptimize(const T &value) {
  asm volatile("" : "+m"(const_cast<T &>(value)));
}

// Key has been accessed again (not first occurance)
// Reserve or determine if capacity conflict.
// If present, mark reserved; don't advance set
// - Reserve:  Mark 1 from last k -> (k, t-1).
// If !present, generate pull & advance set.
// - Pull:  Mark 1 at (k, t)
template<typename Key>
auto SimMIN<Key>::update(const Key& k, const Time& t) {
  History& hist = reserved_[k];
  if (hist.size() > 0 && barrier_ <= hist.back().second) {
    hits_++;

    Reservation& res = hist.back();
    size_t column_begin = res.second;
    size_t column = compulsoryMiss_ + capacityMiss_ - 1;  // t-1

    // Update history to reserve item to t:
//      hist.back().second.second = t;
    res.second = column;

    // Determine if barrier moved:
    size_t last = barrier_;
    for (size_t i = column_begin+1; i <= column; i++ ) {
      const size_t idx = i - trim_offset_;
      assert(idx < capacity_.size());
      // Update count (scoreboard):
      if (++capacity_[idx] >= MAX_) {
        // Take note if barrier moved:
        last = i;
      }
    }

    if (last < barrier_) {
      std::cerr << "WARNING: SIM_MIN's index rolled over size_t!" << std::endl;
    }
    barrier_ = last;
    std::vector<Key> evictSet = trim_to_barrier();
    return std::make_pair(true, evictSet);  // hit
  }
  else {
    size_t column = compulsoryMiss_ + capacityMiss_;  // t
    capacityMiss_++;
    hist.emplace_back(std::make_pair(column,column));
    capacity_.emplace_back(1);
    assert(capacity_.size() == compulsoryMiss_ + capacityMiss_ - trim_offset_);

//    using evictSet_t = typename std::invoke_result_t<decltype(&SimMIN<Key>::trim_to_barrier)>;
    return std::make_pair(false, std::vector<Key>{}); // miss
  }
}


// Delete uneeded metadata:
// - Delete all reservations with an end time before barrier.
// - Update capacity vector, moving barrier to position 0
// - Update trim offset to number of columns 'deleted'
// -- 'time' moves on, but capacity vector's index is adjusted back to 0
template<typename Key>
std::vector<Key> SimMIN<Key>::trim_to_barrier() {
  std::vector<Key> evict_set;
  size_t advance = barrier_ - trim_offset_;
  if (advance == 0) {
    return evict_set;
  }

  max_elements_ = std::max(max_elements_, reserved_.size());
  // For all reservations in history, delete if reservation.end < barrier:
  for (auto& [key, hist] : reserved_) {
    auto cut = hist.begin();
    for (auto it = hist.begin(); it != hist.end(); it++) {
      if (barrier_ > it->second) {
        cut = it + 1;
      }
      else {
        break;
      }
    }
    if (cut == hist.end()) {
      evict_set.push_back(key);
    }
    else if (cut != hist.begin()){
      // should only happen if failed to trim on barrier move...
      hist.erase(hist.begin(), cut);
    }
  }
  for (Key k : evict_set) {
    reserved_.erase(k);
  }

  // Trim capacity vector:
  // - In order for indicies to work, need to keep barrier as element 0.
  auto cut = capacity_.begin() + advance;   // one past range to cut
  capacity_.erase(capacity_.begin(), cut);  // removes [begin, cut)
  trim_offset_ += advance;

#ifdef DEBUG_LOG
  std::cout << "MIN: Barrier hit at " << trim_offset_
            << ", advanced by " << advance << " demands." << std::endl;
#endif
  return evict_set;
}


#endif // SIM_MIN_HPP
