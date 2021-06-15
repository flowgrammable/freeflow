#include "types.hpp"

/// Timespec arithmatic
// TODO: replace this with chrono duration...
timespec operator-(const timespec& lhs, const timespec& rhs) {
  constexpr auto NS_IN_SEC = 1000000000LL;
  timespec result;

  /* Compute the time. */
  result.tv_sec = lhs.tv_sec - rhs.tv_sec;
  result.tv_nsec = lhs.tv_nsec - rhs.tv_nsec;

  /* Perform the carry. */
  if (result.tv_nsec < 0) {
//    auto carry = result.tv_nsec / NS_IN_SEC + 1;
//    result.tv_nsec += NS_IN_SEC * carry;
//    result.tv_sec -= carry;
    result.tv_nsec += NS_IN_SEC;
    result.tv_sec -= 1;
  }
  else if (result.tv_nsec >= NS_IN_SEC) {
//    auto carry = result.tv_nsec / NS_IN_SEC;
//    result.tv_nsec -= NS_IN_SEC * carry;
//    result.tv_sec += carry;
    result.tv_nsec -= NS_IN_SEC;
    result.tv_sec += 1;
  }

  return result;
}

timespec operator+(const timespec& lhs, const timespec& rhs) {
  constexpr auto NS_IN_SEC = 1000000000LL;
  timespec result;

  /* Compute the time. */
  result.tv_sec = lhs.tv_sec + rhs.tv_sec;
  result.tv_nsec = lhs.tv_nsec + rhs.tv_nsec;

  /* Perform the carry. */
  if (result.tv_nsec < 0) {
    result.tv_nsec += NS_IN_SEC;
    result.tv_sec -= 1;
  }
  else if (result.tv_nsec >= NS_IN_SEC) {
    result.tv_nsec -= NS_IN_SEC;
    result.tv_sec += 1;
  }

  return result;
}

std::ostream& operator<<(std::ostream& os, const timespec& ts) {
  os << '{' << ts.tv_sec << "s," << ts.tv_nsec << "ns}";
  return os;
}

std::string to_string(const timespec& ts) {
  const std::string ns = std::to_string(ts.tv_nsec);
  return std::to_string(ts.tv_sec) + ":" +
         std::string(9 - ns.length(), '0') + ns;
}
