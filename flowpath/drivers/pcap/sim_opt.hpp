#ifndef SIM_OPT_HPP
#define SIM_OPT_HPP

#include <vector>
#include <set>
#include <iterator>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <type_traits>
#include <sstream>

#include "types.hpp"
#include "util_container.hpp"

// Google's Abseil C++ library:
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"


// Encapsulates actual OPT reservation.
// - Tracks an actual reservation lifetime.
class ReservationOPT {
public:
  enum class Action {
    Bypass,
    Reserve
  };

  using OPT_Time = size_t;
  ReservationOPT(); // default construct to invalid reservation
  ReservationOPT(OPT_Time first, OPT_Time last, int references);

  explicit operator bool() const;   // if sucessfully marked as reserved.
  OPT_Time first() const;
  OPT_Time last() const;

  OPT_Time extend(OPT_Time t);

private:
  OPT_Time tDemand_;  // t where reservation started.
  OPT_Time tLast_;    // t where reservation ended.
  int refCount_ = 0;  // number of references during reservation lifetime.
  Action valid_ = Action::Bypass;    // notes if met reuse threshold.
};


// Encapsulates OPT reservation qualification complexity.
// - MIN page replacement reserves on every demand (BypassThreshold == 1)
// - OPT does not offically reserve entry until first re-reference (BypassThreshold == 2)
// - OPT's re-reference threshold may be higher depending on use-case.
// - Stores time entry was potentially brought in (tDemand).
// - Provides interface to ask if entry is eligable for reservation and returns
//   relevant time range for reservation.
template<size_t BypassThreshold>
class HistoryOPT {
  static_assert (BypassThreshold >= 1, "BypassThreshold must be >= 1");
public:
  using OPT_Time = size_t;
  HistoryOPT(OPT_Time t);

  bool eligible(OPT_Time barrier) const;
  std::optional<OPT_Time> update(OPT_Time t, OPT_Time barrier);
  auto checkExpired(OPT_Time t);

private:
  void internal_update(OPT_Time t);
  OPT_Time internal_extend(OPT_Time t);

  // Member Variables:
  size_t elements_ = 0;   // MAX value is really BypassThreshold
  std::array<OPT_Time, BypassThreshold> references_;
  //  ReservationOPT res_;
  std::vector<ReservationOPT> res_; // history of reservations between trimming
};


// Construction invariant requires at least 1 entry in array.
template<size_t BypassThreshold>
HistoryOPT<BypassThreshold>::HistoryOPT(OPT_Time t) {
  internal_update(t);
  if (BypassThreshold == 1) {
    internal_extend(t);
  }
}

// Extracts any reservations expired (last reference past barrier).
// - only returns last (unconfirmed) reservation if last reference also exceeds barrier.
template<size_t BypassThreshold>
auto HistoryOPT<BypassThreshold>::checkExpired(OPT_Time barrier) {
  const auto min = std::min_element(std::begin(references_), std::end(references_));
  if (res_.empty() || *min < barrier) {
    // All reservations have expired:
    // TODO: create 'bypass' reservation if didn't meet eligibility...
    // TODO: need way of reporting 'bypass' for ineligible ranges?
    std::vector<ReservationOPT> tmp;
    res_.swap(tmp);
    return tmp; // return [first, last] reservations
  }
  else {
    // Last reservation is still alive:
    std::vector<ReservationOPT> tmp = {res_.back()};  // keep latest reservation
    res_.swap(tmp);
    tmp.pop_back();
    return tmp; // return [first, last) reservations
  }
}

// Check if eligable to reserve in OPT table.
// barrier: last t where cache was at capacity.
template<size_t BypassThreshold>
bool HistoryOPT<BypassThreshold>::eligible(OPT_Time barrier) const {
  if (elements_ == BypassThreshold) {
    const auto min = std::min_element(std::begin(references_), std::end(references_));
    return *min >= barrier;  // alive if oldest element >= barrier
  }
  return false; // not eligible until full.
}

// Helper function to notes latest reference.
// - Fills up array to BypassThreshold.
// - Overwrites oldest access when array is full.
template<size_t BypassThreshold>
void HistoryOPT<BypassThreshold>::internal_update(OPT_Time t) {
  if (elements_ < BypassThreshold) {
    references_[elements_++] = t;
  }
  else {
    auto min = std::min_element(std::begin(references_), std::end(references_));
    *min = t;
  }
}

// Helper function to unconditionally create/extend reservation.
// - Assumes eligible to reserve in OPT table.
// - Returns first t needed to mark range in OPT capacity vector.
template<size_t BypassThreshold>
typename HistoryOPT<BypassThreshold>::OPT_Time
HistoryOPT<BypassThreshold>::internal_extend(OPT_Time t) {
  OPT_Time first;
  if (res_.empty() || !res_.back()) {
    // New reservation:
    const auto min = std::min_element(std::begin(references_), std::end(references_));
    res_.emplace_back(*min, t, std::size(references_));
    first = *min;
  }
  else {
    // Extend reservation:
    first = res_.back().extend(t);
  }
  return first;
}

// Extends or creates new reservation reservation if eligible.
// - Returns first t needed to mark range in OPT capacity vector.
template<size_t BypassThreshold>
std::optional<typename HistoryOPT<BypassThreshold>::OPT_Time>
HistoryOPT<BypassThreshold>::update(OPT_Time t, OPT_Time barrier) {
  internal_update(t);
  if (eligible(barrier)) {
    return internal_extend(t);  // reservation extended, returns last t reserved.
  }
  return {};  // reservation wasn't eligible to extend.
}



// SimOPT differs to SimMIN by not reserving entry until subsequent reuse.
// - on first reference, Reservation is not counted in capacity vector.
// - on re-reference, mark from reservation_end to current time.
// - last entry in reservation should be considered a dead after current time
//   exceeds reservation_end.
template<typename Key>
class SimOPT {
  using Count = uint32_t; // used for capacity vector
  using Time = timespec;  // maybe throw out?
  using OPT_Time = size_t;
//  using Hits = int;
//  using Point = std::pair<size_t, Time>;  // miss-index, real-time
//  using Reservation = std::tuple<MIN_Time, MIN_Time, Hits>; // Cache entry live-time
//  using Reservation = std::pair<MIN_Time, MIN_Time>; // Cache entry live-time
//  using Entry = std::tuple<Reservation, Hits>;
//  using History = std::vector<HistoryOPT>;

public:
  static constexpr size_t BypassThreshold = 1;

  SimOPT(size_t entries);

  void insert(const Key& k, const Time& t);
  bool update(const Key& k, const Time& t);
  void remove(const Key& k);  // does nothing...
  auto evictions() {return trim_to_barrier();}

  size_t get_size() const {return MAX_;}
  uint64_t get_hits() const {return hits_;}
  uint64_t get_capacity_miss() const {return capacityMiss_;}
  uint64_t get_compulsory_miss() const {return compulsoryMiss_;}
  size_t get_max_elements() const {return max_elements_;}
  std::string print_stats() const;

private:
  auto internal_insert(const Key& k);
  auto trim_to_barrier();

  // Member Variables:
  const size_t MAX_;
  OPT_Time barrier_ = 0;      // absolute indexing (adjust with trim_offset)
  OPT_Time trim_offset_ = 0;  // offset for relative indexing
  OPT_Time t_ = 0;            // tracks opt column (time).
  std::unordered_map<Key, HistoryOPT<BypassThreshold>> reserved_;
  std::vector<Count> capacity_;  // count within columns in OPT table.

  // Stats (summary from columns in OPT table):
  uint64_t hits_ = 0;
  uint64_t capacityMiss_ = 0;
  uint64_t compulsoryMiss_ = 0;
//  uint64_t invalidates_ = 0;
  size_t max_elements_ = 0; // replace with average time to decision?
};


/////////////////////////////
/// SimOPT Implementation ///
/////////////////////////////

template<typename Key>
SimOPT<Key>::SimOPT(size_t entries) : MAX_(entries) {
  reserved_.reserve(entries*16);  // prepare for roughly 10:1 flow:active ratio
}


// Helper function to create reservation tracking object upon known miss.
template<typename Key>
auto SimOPT<Key>::internal_insert(const Key& k) {
  size_t column = compulsoryMiss_ + capacityMiss_;  // t

  // emplace returns std::pair<iterator, bool success>
  auto status = reserved_.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(k),
                                  std::forward_as_tuple(column));
  assert(status.second);  // ensure always inserts

  constexpr int reserved = BypassThreshold == 1 ? 1 : 0;
  capacity_.emplace_back(reserved);  // Wait until subsequent re-reference to reserve.
  assert(capacity_.size() == compulsoryMiss_ + capacityMiss_ - trim_offset_);
}


// Key has been created (first occurance)
// - Pull: Insert new column (time advances), but don't reserve until re-use.
template<typename Key>
void SimOPT<Key>::insert(const Key& k, const Time& t) {
  compulsoryMiss_++;
  internal_insert(k);
}

// Key has been accessed again (not first occurance)
// Reserve or determine if capacity conflict.
// If present, mark reserved; don't advance set
// - Reserve: Mark 1 from last k -> (k, t).
// If !present, generate pull & advance set.
// - Pull: Insert new column (time advances), but don't reserve until re-use.
template<typename Key>
bool SimOPT<Key>::update(const Key& k, const Time& t) {
  size_t column = compulsoryMiss_ + capacityMiss_;  // t
//  HistoryOPT<BypassThreshold>& hist = reserved_[k];
  auto res_it = reserved_.find(k);
  if (res_it != std::end(reserved_)) {
    auto last = res_it->second.update(column, barrier_);
    if (last) {
      hits_++;
      // Count reservation and note if barrier moved:
      for (OPT_Time i = *last; i < column; i++) {
        const OPT_Time idx = i - trim_offset_;
        // Update count (scoreboard):
        if (++capacity_.at(idx) >= MAX_) {
          // Note if barrier moved (cache demand reached):
          if (i < barrier_) {
            std::cerr << "WARNING: SIM_OPT's index rolled over size_t!" << std::endl;
          }
          barrier_ = i;  // Gain insight to eviciton/s (when barrier moved).
        }
      }
      return true;  // hit
    }
    else {
      // reservation not confirmed, not yet know if hit or miss...
      return false;   // TODO: need to follow up somehow...
    }
  }
  capacityMiss_++;  // Not eligible, known capacity miss
  internal_insert(k);
  return false; // miss
}


// Delete uneeded metadata:
// - Delete all reservations with an end time before barrier.
// - Update capacity vector, moving barrier to position 0
// - Update trim offset to number of columns 'deleted'
// -- 'time' moves on, but capacity vector's index is adjusted back to 0
template<typename Key>
auto SimOPT<Key>::trim_to_barrier() {
  std::set<Key> evict_set, bypass_set;
  size_t advance = barrier_ - trim_offset_;
  if (advance == 0) {
    return std::make_pair(evict_set, bypass_set);
  }
  max_elements_ = std::max(max_elements_, reserved_.size());

  // Delete all Entry where reservation.end < barrier:
  // - if no entries are left, delete entire reservation:
  for (auto res_it = std::begin(reserved_); res_it != std::end(reserved_);) {
    const Key key = std::get<const Key>(*res_it);
    HistoryOPT<BypassThreshold>& hist = std::get<HistoryOPT<BypassThreshold>>(*res_it);

    std::vector<ReservationOPT> expired = hist.checkExpired(barrier_);
    for (const auto& e : expired) {
      if (e) {
        evict_set.insert(key);
      }
      else {
        bypass_set.insert(key);   // TODO: this doesn't work as expected.
      }
    }
//    if (!hist.alive(barrier_)) {
//      res_it = reserved_.erase(res_it);   // ++res_it
//    }
//    else {
//      ++res_it;
//    }


//    auto hist_it = hist.begin();
//    do {  // Advance to cut point:
//      // Common case is 1 entry history, but multiple entries are possible.
//      if (barrier_ < (hist_it++)->last()) {
//        // Stop on first valid reservation.
//        break;  // cut point (one past elements to delete)
//      }
//    } while (hist_it != hist.end());

//    if (hist_it == hist.end()) {
//      // All Entries have expired, delete entire History.
//      auto references = hist_it->references_;
//      if (references > 1) {
//        evict_set.insert(key);
//      }
//      else if (references == 1) {
//        bypass_set.insert(key);
//      }
//      else {
//        throw std::runtime_error("Reservation reference count shouldn't be <= 0.");
//      }
//      res_it = reserved_.erase(res_it);   // ++res_it
//    }
//    else {
//      // Old entry expired, trim to latest history entry.
//      if (std::distance(hist.begin(), hist_it) > 1) {
//        // More than one entry has expired.
//        // - This shouldn't happen if barrier is kept updated after every miss.
//        std::cerr << "MinOPT: More than entry in reservation (shouldn't happen if barrier is kept updated after every miss)." << std::endl;
//      }
//      hist.erase(hist.begin(), hist_it);
//      ++res_it;
//    }
  }

  // Trim capacity vector:
  // - In order for indicies to work, need to keep barrier as element 0.
  auto cut = capacity_.begin() + advance;   // one past range to cut
  capacity_.erase(capacity_.begin(), cut);  // removes [begin, cut)
  trim_offset_ += advance;

#ifdef DEBUG_LOG
  std::cout << "OPT: Barrier hit at " << trim_offset_
            << ", advanced by " << advance << " demands." << std::endl;
#endif
  return std::make_pair(evict_set, bypass_set);
}


template<typename Key>
std::string SimOPT<Key>::print_stats() const {
  std::stringstream ss;
  ss << "SimOPT Cache Size: " << get_size() << '\n';
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


#endif // SIM_OPT_HPP
