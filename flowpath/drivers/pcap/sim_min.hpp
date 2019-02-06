#ifndef SIM_MIN_HPP
#define SIM_MIN_HPP

#include <vector>

#include "types.hpp"

// Google's Abseil C++ library:
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"


template<typename Key>
class SimMin {
  using Count = uint32_t;
  using Time = timespec;  // maybe throw out?
  using Point = std::pair<size_t,Time>;  // miss-index, real-time
//  using Reservation = std::pair<Point,Point>;
  using Reservation = std::pair<size_t,size_t>;
  using History = absl::InlinedVector<Reservation,1>;

public:
  SimMin(size_t entries) : MAX_(entries) {
    reserved_.reserve(entries*2);
  }

  bool insert(const Key& k, const Time& t) {
    // Key has been created (first occurance)
    // - Pull:  Mark 1 at (k, t).

    compulsoryMiss_++;
    size_t column = compulsoryMiss_ + capacityMiss_;
//    auto status = reserved_.emplace(std::piecewise_construct,
//                                    std::forward_as_tuple(k),
//             std::forward_as_tuple(k, History(1, std::make_pair(column,column))) );
//    assert(status.second);
    History& hist = reserved_[k];
    hist.emplace_back(std::make_pair(column,column));
    capacity_.emplace_back(1);
  }

  bool update(const Key& k, const Time& t) {
    // Key has been accessed again (not first occurance)
    // Reserve or determine if capacity conflict.
    // If present, mark reserved; don't advance set
    // - Reserve:  Mark 1 from last k -> (k, t-1).
    // If !present, generate pull & advance set.
    // - Pull:  Mark 1 at (k, t)

    History& hist = reserved_[k];
    if (hist.size() > 0 && barrier_ <= hist.back().second) {
      hits_++;

      size_t column_begin = hist.back().second;
      size_t column = compulsoryMiss_ + capacityMiss_;

      // Update history to reserve item to t:
//      hist.back().second.second = t;
      hist.back().second = column;

      // determine if barrier changed...
      size_t last = barrier_;
      for (size_t i = column_begin+1; i <= column; i++ ) {
        if (++capacity_[i] >= MAX_) {
          last = i;
        }
      }
      barrier_ = last;
    }
    else {
      capacityMiss_++;
      size_t column = compulsoryMiss_ + capacityMiss_;
      hist.emplace_back(std::make_pair(column,column));
      capacity_.emplace_back(1);
    }
  }

  bool remove(const Key& k) {
    // Key has been invalidated (never to be seen again)
    // Even if touched within Epoc, mark invalidated.
    // - Evict: Mark 0 at (k, t-1)??
  }

private:
  const size_t MAX_;
  size_t barrier_ = 0;
  absl::flat_hash_map<Key, History> reserved_;
  std::vector<Count> capacity_;  // count within columns in MIN table.

  // Stats (instead of columns in MIN table):
  uint64_t hits_ = 0;
  uint64_t capacityMiss_ = 0;
  uint64_t compulsoryMiss_ = 0;
//  uint64_t invalidates_ = 0;
};


#endif // SIM_MIN_HPP
