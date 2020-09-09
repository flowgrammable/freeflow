#ifndef FP_TYPES_HPP
#define FP_TYPES_HPP

// Defines common types in the system.

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <string>
#include <utility>
#include <type_traits>
#include <algorithm>
#include <iostream>
#include <cassert>

//// Simple saturating conversion helper ////
template<typename T, typename U>
constexpr T ClampDown(U u) {
  return std::clamp(u, U(std::numeric_limits<T>::min()), U(std::numeric_limits<T>::max()));
}


//// Saturating Counter ////
template<size_t bits>
class ClampedInt {
  static_assert(bits > 0 && bits < 64,
                "Restricted to signed counters of bitwidth 1 to 63");

  using SmallInt = typename std::conditional<bits<=8, int8_t, int16_t>::type;
  using LargeInt = typename std::conditional<bits<=32, int32_t, int64_t>::type;
  static constexpr size_t SHIFT = std::numeric_limits<int64_t>::digits - (bits-1);  // bits-1 because storing signed values

public:
  using IntType_t = typename std::conditional<bits<=16, SmallInt, LargeInt>::type;
  static constexpr int64_t MAX = std::numeric_limits<int64_t>::max() >> SHIFT;
  static constexpr int64_t MIN = std::numeric_limits<int64_t>::min() >> SHIFT;
  static constexpr uint64_t RANGE = std::numeric_limits<uint64_t>::max() >> (SHIFT+1);

  constexpr ClampedInt() = default;
  constexpr ClampedInt(int64_t i);

  constexpr ClampedInt& operator++();
  constexpr ClampedInt& operator--();
  constexpr ClampedInt& operator=(const ClampedInt&);
  constexpr int64_t get() const;
  constexpr uint64_t unsignedDistance() const;

  constexpr bool operator<(const ClampedInt&) const;
  constexpr bool operator>(const ClampedInt&) const;
  constexpr bool operator<=(const ClampedInt&) const;
  constexpr bool operator>=(const ClampedInt&) const;
  constexpr bool operator==(const ClampedInt&) const;

private:
  IntType_t value_;
};


template<size_t bits>
constexpr ClampedInt<bits>::ClampedInt(int64_t i) {
  assert(i >= MIN);
  assert(i <= MAX);
  value_ = std::clamp(i, MIN, MAX);
}

template<size_t bits>
constexpr ClampedInt<bits>& ClampedInt<bits>::operator++() {
  if (value_ < MAX) {
    ++value_;
  }
  assert(value_ >= MIN);
  assert(value_ <= MAX);
  return *this;
}

template<size_t bits>
constexpr ClampedInt<bits>& ClampedInt<bits>::operator--() {
  if (value_ > MIN) {
    --value_;
  }
  assert(value_ >= MIN);
  assert(value_ <= MAX);
  return *this;
}

template<size_t bits>
constexpr ClampedInt<bits>& ClampedInt<bits>::operator=(const ClampedInt& rhs) {
  value_ = rhs.value_;
  return *this;
}

template<size_t bits>
constexpr int64_t ClampedInt<bits>::get() const {
  return int64_t(value_);
}

template<size_t bits>
constexpr uint64_t ClampedInt<bits>::unsignedDistance() const {
  return uint64_t(value_) - uint64_t(MIN);
}

template<size_t bits>
constexpr bool ClampedInt<bits>::operator<(const ClampedInt& rhs) const {
  return value_ < rhs.value_;
}

template<size_t bits>
constexpr bool ClampedInt<bits>::operator>(const ClampedInt& rhs) const {
  return value_ > rhs.value_;
}

template<size_t bits>
constexpr bool ClampedInt<bits>::operator<=(const ClampedInt& rhs) const {
  return !(value_ > rhs.value_);
}

template<size_t bits>
constexpr bool ClampedInt<bits>::operator>=(const ClampedInt& rhs) const {
  return !(value_ < rhs.value_);
}

template<size_t bits>
constexpr bool ClampedInt<bits>::operator==(const ClampedInt& rhs) const {
  return value_ == rhs.value_;
}



//// Tuple Serilaization ////
// Compile-time tuple detection:
template<typename> struct is_tuple: std::false_type {};
template<typename ...T> struct is_tuple<std::tuple<T...>>: std::true_type {};

// http://aherrmann.github.io/programming/2016/02/28/unpacking-tuples-in-cpp14/
template<class F, size_t... Is>
constexpr auto index_apply_impl(F func, std::index_sequence<Is...>) {
  return func(std::integral_constant<size_t, Is>{}...);
}
template<size_t N, class F>
constexpr auto index_apply(F func) {
  return index_apply_impl(func, std::make_index_sequence<N>{});
}

//// Element and target specializations ////
// Serialize unique (non-padded) object to output stream (Binary):
// std::enable_if<std::is_convertable<Target, std::basic_ostream>::value, Target>::type& out
template <typename Target, typename T>
typename std::enable_if<std::has_unique_object_representations_v<T>, void>::type
serialize(Target& out, const T& x) {
  out.write(reinterpret_cast<const char*>(&x), sizeof(x));
}

// Serialize element to std::basic_ostream:
//template <typename T>
//void serialize(std::basic_ostream& s, const T& x) {
//  s.append(reinterpret_cast<const char*>(&x), sizeof(x));
//}

// Serialize element to std::string:
template <typename T>
void serialize(std::string& s, const T& x) {
  s.append(reinterpret_cast<const char*>(&x), sizeof(x));
}

// Serialize pairs - gets pair elements:
template <typename Target, typename T1, typename T2>
void serialize(Target& out, const std::pair<T1, T2>& p) {
  serialize(out, p.first);
  serialize(out, p.second);
}

//// Tuple unpacking ////
// Forward declaration of top-level tuple template specialization
// - Required to enable recursive tuple unpacking.
template <typename Target, typename... Ts>
void serialize(Target&, const std::tuple<Ts...>&);

// Serialize tuple helper - gets tuple element from index_sequence:
template <typename Target, typename Tuple, size_t... Is>
void serialize_tuple(Target& out, const Tuple& t, std::index_sequence<Is...>) {
  (serialize(out, std::get<Is>(t)), ...);
}

// Serialize tuples - generates index sequence:
template <typename Target, typename... Ts>
void serialize(Target& out, const std::tuple<Ts...>& t) {
  if (out) {
    serialize_tuple(out, t, std::index_sequence_for<Ts...>{});
  }
}

template<class Ch, class Tr, class... Ts>
auto& operator<<(std::basic_ostream<Ch, Tr>& os, const std::tuple<Ts...>& t) {
    serialize_tuple(os, t, std::index_sequence_for<Ts...>{});
    return os;
}

//// Generic for_each element in tuple ////
template <std::size_t... Idx>
auto make_index_dispatcher(std::index_sequence<Idx...>) {
    return [] (auto&& f) { (f(std::integral_constant<std::size_t,Idx>{}), ...); };
}

template <std::size_t N>
auto make_index_dispatcher() {
    return make_index_dispatcher(std::make_index_sequence<N>{});
}

template <typename Tuple, typename Func>
void for_each(Tuple&& t, Func&& f) {
    constexpr auto n = std::tuple_size<std::decay_t<Tuple>>::value;
    auto dispatcher = make_index_dispatcher<n>();
    dispatcher([&f,&t](auto idx) { f(std::get<idx>(std::forward<Tuple>(t))); });
}

// example for_each lambdas...
//      auto serialize = [&](auto&& x) {
//        flowStats.write(reinterpret_cast<const char*>(&x), sizeof(x));
//      };
//      std::apply([&](auto& ...x){(..., serialize(x));}, fkt);
//      for_each(fkt, [&](auto&& e) {serialize(flowStats, e);});



/// Timespec arithmatic
timespec operator-(const timespec& lhs, const timespec& rhs);
timespec operator+(const timespec& lhs, const timespec& rhs);
std::ostream& operator<<(std::ostream& os, const timespec& ts);
std::string to_string(const timespec& ts);


namespace fp
{
// An integer value of 8 bits.
using Byte = uint8_t;


// FIXME: What are these?
using begin = Byte* (*)();
using end = Byte* (*)();


// packed 24 bit integer
// wraps a 32 bit integer
#pragma pack(push, 1)
struct uint24_t {
  uint24_t()
    : i(0)
  { }

  uint24_t(unsigned int i)
    : i(i)
  { }

  unsigned int i : 24;
};
#pragma pack(pop)


// packed 48 bit integer
// wraps a 64 bit integer
#pragma pack(push, 1)
struct uint48_t {
  uint48_t()
    : i(0)
  { }

  uint48_t(uint64_t i)
    : i(i)
  { }

  uint64_t i : 48;
};
#pragma pack(pop)


} // end namespace fp


#endif
