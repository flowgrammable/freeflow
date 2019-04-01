#ifndef FP_TYPES_HPP
#define FP_TYPES_HPP

// Defines common types in the system.

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <string>
#include <utility>
#include <type_traits>


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
  serialize_tuple(out, t, std::index_sequence_for<Ts...>{});
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
