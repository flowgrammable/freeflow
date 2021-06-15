#ifndef SIM_MIN_HPP
#define SIM_MIN_HPP

#include <vector>
#include <set>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <type_traits>
#include <sstream>

#include "types.hpp"
#include "util_container.hpp"

// Google's Abseil C++ library:
#include "absl/container/flat_hash_map.h"
//#include "absl/container/flat_hash_set.h"
//#include "absl/container/inlined_vector.h"

// Global type definitions:
using Time = timespec;
using MIN_Time = size_t;

// Global sim time references:
extern Time SIM_TS_LATEST;

struct Reservation {
  Reservation();
  explicit Reservation(Time t, MIN_Time col);

  MIN_Time last_col() const;
  Time last_ts() const;

  bool covers(MIN_Time col) const;
  bool strictlyCovers(MIN_Time col) const;

  MIN_Time duration_minTime() const;
  Time duration_time() const;

  void extend(Time t, MIN_Time col);

  // Times stored are inclusive [first, last]
  std::pair<MIN_Time, MIN_Time> reservation_min_;
  std::pair<Time, Time> reservation_time_;
  uint64_t hits = 0;
};

using History = std::vector<Reservation>;


template<typename Key>
class SimMIN {
public:
  static constexpr bool ENABLE_DELAY_STATS = false;

  SimMIN(size_t entries);

  void insert(const Key& k, const Time& t);
  bool update(const Key& k, const Time& t);
  void remove(const Key& k);  // does nothing...
  std::pair<std::set<Key>, std::set<Key>> evictions() {return trim_to_barrier();}
  std::pair<std::map<Key, History>, std::set<Key>> eviction_spans() {return trim_to_barrier_spans();}

  size_t get_size() const {return ENTRIES_;}
  uint64_t get_hits() const {return hits_;}
  uint64_t get_capacity_miss() const {return capacityMiss_;}
  uint64_t get_compulsory_miss() const {return compulsoryMiss_;}
  size_t get_max_elements() const {return max_rows_;}
  std::string print_stats() const;

private:
  std::pair<std::map<Key, History>, std::set<Key>> trim_to_barrier_spans();
  std::pair<std::set<Key>, std::set<Key>> trim_to_barrier();

  const size_t ENTRIES_;
  MIN_Time barrier_ = 0;      // absolute indexing (adjust with trim_offset)
  MIN_Time trim_offset_ = 0;  // offset for relative indexing
  absl::flat_hash_map<Key, History> reserved_;
  std::vector<uint32_t> capacity_;  // count within columns in MIN table.

  // Stats (summary from columns in MIN table):
  uint64_t hits_ = 0;
  uint64_t capacityMiss_ = 0;
  uint64_t compulsoryMiss_ = 0;

  // Belady Matrix Dimension Stats:
  size_t max_rows_ = 0;     // y-dimension (cache entries tracked)
  size_t max_columns_ = 0;  // x-dimension (tracked inserts: MIN_TIME)
  Time max_duration_{};     // x-dimension decision latency in real time
};


/////////////////////////////
/// SimMin Implementation ///
/////////////////////////////

template<typename Key>
SimMIN<Key>::SimMIN(size_t entries) : ENTRIES_(entries) {
  reserved_.reserve(entries*2);
}


// Key has been created (first occurance)
// - Pull:  Mark 1 at (k, t).
template<typename Key>
void SimMIN<Key>::insert(const Key& k, const Time& t) {
  MIN_Time column = compulsoryMiss_ + capacityMiss_;  // t
  compulsoryMiss_++;

  History& hist = reserved_[k];
  hist.emplace_back(t, column);
  capacity_.emplace_back(1);
  assert(capacity_.size() == compulsoryMiss_ + capacityMiss_ - trim_offset_);
}


// Key has been accessed again (not first occurance)
// Reserve or determine if capacity conflict.
// If present, mark reserved; don't advance set
// - Reserve:  Mark 1 from last k -> (k, t-1).
// If !present, generate pull & advance set.
// - Pull:  Mark 1 at (k, t)
template<typename Key>
bool SimMIN<Key>::update(const Key& k, const Time& t) {
  History& hist = reserved_[k];
//  if (hist.size() > 0 && barrier_ <= hist.back().second) {
  if (!hist.empty() && hist.back().covers(barrier_)) {
    hist.back().hits++; // hits per reservation
    hits_++;            // total cache hits

    Reservation& res = hist.back();
    MIN_Time column_begin = res.last_col();
    MIN_Time column = compulsoryMiss_ + capacityMiss_ - 1;  // t-1

    // Update history to reserve item to t:
    res.extend(t, column);

    // Determine if barrier moved:
    MIN_Time last = barrier_;
    for (MIN_Time i = column_begin+1; i <= column; i++ ) {
      const MIN_Time idx = i - trim_offset_;
      assert(idx < capacity_.size());
      // Update count (scoreboard):
      if (++capacity_[idx] >= ENTRIES_) {
        // Note if barrier moved:
        last = i;
      }
    }

    if (last < barrier_) {
      std::cerr << "WARNING: SIM_MIN's index rolled over size_t!" << std::endl;
    }

    // Gain insight to eviciton/s (if barrier moved):
    barrier_ = last;
    return true;  // hit
  }
  else {
    // miss, re-insert into cache:
    MIN_Time column = compulsoryMiss_ + capacityMiss_;  // t
    capacityMiss_++;
    hist.emplace_back(t, column);
    capacity_.emplace_back(1);
    assert(capacity_.size() == compulsoryMiss_ + capacityMiss_ - trim_offset_);
    return false; // miss
  }
}


// Delete uneeded metadata:
// - Delete all reservations with an end time before barrier.
// - Update capacity vector, moving barrier to position 0
// - Update trim offset to number of columns 'deleted'
// -- 'time' moves on, but capacity vector's index is adjusted back to 0
template<typename Key>
std::pair<std::map<Key, History>, std::set<Key>>
SimMIN<Key>::trim_to_barrier_spans() {
  std::set<Key> evict_set, keep_set;
  std::map<Key, History> spans;

  MIN_Time advance = barrier_ - trim_offset_;
  if (advance == 0) {
    return std::make_pair(spans, keep_set);
  }
  static Time tempSum{};

  max_rows_ = std::max(max_rows_, reserved_.size());
  // For all reservations in history, delete if reservation.end < barrier:
  for (auto& [key, hist] : reserved_) {
    auto cut = hist.begin();
    for (auto it = hist.begin(); it != hist.end(); it++) {
//      if (it->first <= barrier_ && it->second >= barrier_) {
      if (it->strictlyCovers(barrier_)) {
        // if reservation was extended: [first <= barrier_ <= second]
        keep_set.insert(key);   // confirm as good entry to keep
        if (ENABLE_DELAY_STATS) {
          std::cout << key << " Belady marked Keep: " << SIM_TS_LATEST - it->last_ts() << std::endl;
        }
      }
      else if (barrier_ > it->last_col()) {
        // if reservation was dormant: [first <= second < barrier_]
        cut = it + 1;   // mark for reclaiming
      }
//      else {
//        // Reservation is newer than barrier_:  [barrier < first]
//        assert(false);
//        break;
//      }
    }
    if (cut == hist.end()) {
      evict_set.insert(key);
      if (ENABLE_DELAY_STATS) {
        std::cout << key << " Belady marked Evict: " << SIM_TS_LATEST - hist.back().last_ts() << std::endl;
      }
    }
    else if (cut != hist.begin()){
      // FIXME: dead code?  should only happen if failed to trim on barrier move...
      hist.erase(hist.begin(), cut);
    }
  }

  for (const Key& k : evict_set) {
    spans.emplace(k, std::move(reserved_[k]));
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
  return std::make_pair(spans, keep_set);
}


// Emulates old trim_to_barrier interface where just evict and keep sets are returned.
template<typename Key>
std::pair<std::set<Key>, std::set<Key>> SimMIN<Key>::trim_to_barrier() {
  std::set<Key> evict_set;
  auto [spans, keep_set] = trim_to_barrier_spans();
  for (const auto& [k,v] : spans) {
    evict_set.insert(k);
  }
  return std::make_pair(evict_set, keep_set);
}


template<typename Key>
std::string SimMIN<Key>::print_stats() const {
  std::stringstream ss;
  ss << "SimMIN Cache Size: " << get_size() << '\n';
  ss << " - Hits: " << get_hits() << '\n';
  ss << " - Miss (Compulsory): " << get_compulsory_miss() << '\n';
  ss << " - Miss (Capacity): " << get_capacity_miss() << '\n';
  ss << " - Max elements between barrier: " << get_max_elements();
  // TODO:
  // print stats on element lifetime, average + std-dev hits over lifetime
  // print distribution on ref count over lifetime
  // print distribution on time count over lifetime
  return ss.str();
}


#endif // SIM_MIN_HPP
