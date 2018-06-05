#ifndef UTIL_VIEW_HPP
#define UTIL_VIEW_HPP

//#include "system.hpp"
//#include "dataplane.hpp"
//#include "application.hpp"
//#include "port_table.hpp"
//#include "port.hpp"
//#include "port_pcap.hpp"
//#include "context.hpp"
/////#include "packet.hpp" // temporary for sizeof packet
/////#include "context.hpp" // temporary for sizeof context
//#include "util_bitmask.hpp"

//#include <string>
//#include <iostream>
//#include <stdexcept>
//#include <iomanip>
//#include <ctime>
//#include <signal.h>
//#include <type_traits>
//#include <bitset>
//#include <map>
//#include <tuple>
//#include <functional>
//#include <algorithm>
//#include <numeric>
//#include <unordered_map>
//#include <unordered_set>
//#include <vector>
//#include <queue>
//#include <chrono>

//#include <net/ethernet.h>
//#include <netinet/ip.h>
//#include <netinet/in.h>
//#include <netinet/tcp.h>
//#include <arpa/inet.h>
//#ifdef __APPLE__
//#include <machine/endian.h>
//#else
//#include <endian.h>
//#endif

//#include <typeinfo>

namespace util_view {

// Common types with guaranteed widths and signed-ness
using u8 = uint8_t;
//using u16 = uint16_t;
//using u32 = uint32_t;
//using u64 = uint64_t;
//using s8 = int8_t;
//using s16 = int16_t;
//using s32 = int32_t;
//using s64 = int64_t;
//union u128_struct {
//  u64 word[2];
//  struct {
//    u64 upper;
//    u64 lower;
//  };
//};
//using u128 = u128_struct;

template <typename R>
static constexpr typename std::enable_if< std::is_integral<R>::value, R >::type
bitmask(const size_t bitCount)
{
    return static_cast<R>(-(bitCount != 0))
        & (static_cast<R>(-1) >> ((sizeof(R) * CHAR_BIT) - bitCount));
}
template <typename R>
static constexpr typename std::enable_if< std::is_integral<R>::value, R >::type
bytemask(const size_t byteCount)
{
    return static_cast<R>(-(byteCount != 0))
        & (static_cast<R>(-1) >> (sizeof(R) - byteCount));
}


// Keeps track of safe range of access within packet.
class View {
public:
  View(uint8_t* ptr, size_t bytes) :
    begin_(ptr), end_(ptr+bytes),
    beginCP_(begin_), endCP_(end_),
    beginAbs_(begin_), endAbs_(end_) {
  }
  View(uint8_t* ptr, size_t bytes, size_t orig) :
    begin_(ptr), end_(ptr+bytes),
    beginCP_(begin_), endCP_(end_),
    beginAbs_(begin_), endAbs_(ptr+orig) {
  }

  bool sufficient(size_t bytes) const {
    u8* end = static_cast<u8*>(begin_) + bytes;
    return end <= end_;
  }
  template<typename R, size_t bytes = sizeof(R)>
  bool sufficient() const {
    return sufficient(bytes);
  }

  void discard(size_t bytes) {
    if (!sufficient(bytes))
      throw std::out_of_range("Insufficient bytes to discard");

    constrain(bytes);
  }

  void discardEnd(size_t bytes) {
    if (!sufficient(bytes))
      throw std::out_of_range("Insufficient bytes to discard");

    constrainEnd(bytes);
  }

  template<typename R, size_t bytes = sizeof(R)>
  R peek() const {
    if (!sufficient(bytes))
      throw std::out_of_range("Insufficient bytes to read");

    R val = read<R,bytes>();
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  R peekEnd() const {
    if (!sufficient(bytes))
      throw std::out_of_range("Insufficient bytes to read");

    R val = readEnd<R,bytes>();
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  R get() {
    if (!sufficient(bytes))
      throw std::out_of_range("Insufficient bytes to read");

    R val = read<R,bytes>();
    constrain(bytes);
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  R getEnd() {
    if (!sufficient(bytes))
      throw std::out_of_range("Insufficient bytes to read");

    R val = readEnd<R,bytes>();
    constrain(bytes);
    return val;
  }

  // Remaining bytes in View
  template<typename R>
  typename std::enable_if< std::is_integral<R>::value, R >::type
  bytes() const {
    R bytes = static_cast<u8*>(end_) - static_cast<u8*>(begin_);
    return bytes;
  }

  // Remaining bytes in committed view (checkpoint)
  template<typename R>
  typename std::enable_if< std::is_integral<R>::value, R >::type
  committedBytes() const {
    R bytes = static_cast<u8*>(endCP_) - static_cast<u8*>(beginCP_);
    return bytes;
  }

  // Total bytes in original view
  template<typename R>
  typename std::enable_if< std::is_integral<R>::value, R >::type
  absoluteBytes() const {
    R bytes = static_cast<const u8*>(endAbs_) - static_cast<const u8*>(beginAbs_);
    return bytes;
  }

  // Bytes moved in current view since last commit.
  template<typename R>
  typename std::enable_if< std::is_integral<R>::value, R >::type
  pendingBytes() const {
    R bytes = this->committedBytes<R>() - this->bytes<R>();
    return bytes;
  }

  void commit() {
    beginCP_ = begin_;
    endCP_ = end_;
  }

  void rollback() {
    begin_ = beginCP_;
    end_ = endCP_;
  }

  void revert() {
    begin_ = beginAbs_;
    end_ = endAbs_;
  }

private:
  void constrain(size_t bytes) {
    u8* pos = static_cast<u8*>(begin_) + bytes;
    begin_ = pos;
  }

  void constrainEnd(size_t bytes) {
    u8* pos = static_cast<u8*>(end_) - bytes;
    end_ = pos;
  }

  template<typename R, size_t bytes = sizeof(R)>
  typename std::enable_if< std::is_integral<R>::value, R >::type
  read() const {
    R* ptr = static_cast<R*>(begin_);
    R val = *ptr & bytemask<R>(bytes);
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  typename std::enable_if< std::is_compound<R>::value, R >::type
  read() const {
    R* ptr = static_cast<R*>(begin_);
    R val = *ptr;
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  typename std::enable_if< std::is_integral<R>::value, R >::type
  readEnd() const {
    R* ptr = static_cast<R*>(end_) - 1; // read one element (R) back from end...
    R val = *ptr & bytemask<R>(bytes);
    return val;
  }

  template<typename R, size_t bytes = sizeof(R)>
  typename std::enable_if< std::is_compound<R>::value, R >::type
  readEnd() const {
    R* ptr = static_cast<R*>(end_) - 1; // read one element (R) back from end...
    R val = *ptr;
    return val;
  }

private:
  // Current View:
  void* begin_;
  void* end_; // one past end
  // Checkpoint View:
  void* beginCP_;
  void* endCP_; // one past end
  // Absolute View:
  void *const beginAbs_;
  void *const endAbs_;  // one past end
};

} // namespace util_view
#endif
