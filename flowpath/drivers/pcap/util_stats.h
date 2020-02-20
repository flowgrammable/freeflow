#ifndef UTIL_STATS_H
#define UTIL_STATS_H

#include <future>
#include <algorithm>
#include <cmath>

namespace entangle {

/* Calculates the discrete coorilation between two containers.
 * - Best effort calculation up until the min lengh of either range is reached.
 */
template<typename ForwardIt1, typename ForwardIt2>
int64_t discrete_correlation(ForwardIt1 itA, const ForwardIt1 lastA,
                             ForwardIt2 itB, const ForwardIt2 lastB) {
  int64_t corrilation = 0;
  for (; itA != lastA && itB != lastB; ++itA, ++itB) {
    corrilation += itA*itB;
  }
  return corrilation;
}

/* Discrete correlation where coorilation normalized by 'energy' of each
 * signal.
 */
template<typename ForwardIt1, typename ForwardIt2>
double normalized_correlation(ForwardIt1 itA, const ForwardIt1 lastA,
                              ForwardIt2 itB, const ForwardIt2 lastB) {
  int64_t corrilation = 0;
  int64_t energyA = 0;
  int64_t energyB = 0;
  for (; itA != lastA && itB != lastB; ++itA, ++itB) {
    corrilation += (*itA) * (*itB);
    energyA += (*itA) * (*itA);
    energyB += (*itB) * (*itB);
  }
  double energy = std::sqrt(energyA * energyB);
  return double(corrilation) / energy;
}

} // namespace entangle
#endif // UTIL_STATS_H
