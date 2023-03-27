#pragma once

#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace Auxil {
// constexpr-function to compute the smallest power of 2 larger than its
// argument
template <typename uint_>
requires std::unsigned_integral<uint_>
constexpr uint_ pow_two_ceil(uint_ arg) {
  if (std::has_single_bit(arg))
    return arg;
  else
    return pow_two_ceil<uint_>(arg + 1);
};

template <typename I>
requires std::unsigned_integral<I>
constexpr I ceil_(I i1, I i2) {
  if (i1 % i2 == 0)
    return i1;
  else
    return ceil_(static_cast<I>(i1 + 1), i2);
};

template <typename I>
requires std::unsigned_integral<I>
constexpr I divisible_(I i1, I i2) {
  if (i2 % i1 == 0)
    return i1;
  else
    return divisible_(static_cast<I>(i1 + 1), i2);
};

template <typename I>
requires std::unsigned_integral<I>
constexpr I divisible_or_ceil(I i1, I i2) {
  if (i1 < i2)
    return divisible_(i1, i2);
  else
    return ceil_(i1, i2);
};

// metafunction performs typecheck on every type in parameter pack, returns true
// iff check for every single type returns true
template <template <typename Arg> class Check, typename X, typename... XS>
struct check_parameter_pack {
  static constexpr bool value =
      Check<X>::value && check_parameter_pack<Check, XS...>::value;
};

template <template <typename Arg> class Check, typename X>
struct check_parameter_pack<Check, X> {
  static constexpr bool value = Check<X>::value;
};

// generates element-wise additions
template <size_t... index, typename Tuple>
void add_to_tuple_iterate(Tuple& t1, const Tuple& t2,
                          std::index_sequence<index...>) {
  ([&]() { std::get<index>(t1) += std::get<index>(t2); }(), ...);
};

// adds elements of second tuple to respective elements in first tuple
template <typename... Types>
requires check_parameter_pack<std::is_arithmetic, Types...>::value void
add_to_tuple(std::tuple<Types...>& t1, const std::tuple<Types...>& t2) {
  add_to_tuple_iterate(t1, t2, std::make_index_sequence<sizeof...(Types)>{});
};

// takes an unsigned integer type as its argument, returns true if argument is <
// 0, false otherwise
template <typename Sint>
requires std::signed_integral<Sint>
bool is_negative(Sint sint) {
  static constexpr auto shift_by = sizeof(Sint) * 8 - 1;
  if constexpr (std::endian::native == std::endian::little)
    return (std::make_unsigned_t<Sint>)sint >> shift_by == 1;
  else
    return (std::make_unsigned_t<Sint>)sint << shift_by == 1;
}


// concept to enforce a socket wrapper can be called essentially like a POSIX
// socket, via a recv-method
template <typename socketType>
concept readableAsSocket = requires(socketType socket, void* buffer_ptr,
                                    std::uint32_t n_bytes) {
  { socket.recv(buffer_ptr, n_bytes) } -> std::same_as<std::int32_t>;
};

template <typename Tuple,
          template <typename tuple_element> class validate_element, int index>
struct validate_tuple_element {
  static constexpr bool value =
      validate_element<std::tuple_element_t<index, Tuple>>::value &&
      validate_tuple_element<Tuple, validate_element, index - 1>::value;
};

template <typename Tuple,
          template <typename tuple_element> class validate_element>
struct validate_tuple_element<Tuple, validate_element, -1> {
  static constexpr bool value = true;
};

template <typename Tuple,
          template <typename tuple_element> class validate_element>
struct validate_tuple {
  static constexpr bool value =
      validate_tuple_element<Tuple, validate_element,
                             std::tuple_size_v<Tuple> - 1>::value;
};

// function template with overloads for all strint-to-arithmetic conversions in
// standard library overloads for integer-types overload for regular int and
// uint as no conversion function for regular sized uint exists
template <typename Arith>
requires std::integral<Arith> &&(sizeof(Arith) <= 4) Arith
    stoArith(const std::string& arg) {
  return std::stoi(arg);
};

template <typename Arith>
requires std::signed_integral<Arith> &&(sizeof(Arith) == 8) Arith
    stoArith(const std::string& arg) {
  return std::stol(arg);
};

template <typename Arith>
requires std::signed_integral<Arith> &&(sizeof(Arith) > 8) Arith
    stoArith(const std::string& arg) {
  return std::stoll(arg);
};

// overloads for unsigned-integer types
template <typename Arith>
requires std::unsigned_integral<Arith> &&(sizeof(Arith) == 8) Arith
    stoArith(const std::string& arg) {
  return std::stoul(arg);
};

template <typename Arith>
requires std::unsigned_integral<Arith> &&(sizeof(Arith) > 8) Arith
    stoArith(const std::string& arg) {
  return std::stoull(arg);
};

// overloads for floating point types
template <typename Arith>
requires std::floating_point<Arith> &&(sizeof(Arith) <= 4) Arith
    stoArith(const std::string& arg) {
  return std::stof(arg);
};

template <typename Arith>
requires std::floating_point<Arith> &&(sizeof(Arith) == 8) Arith
    stoArith(const std::string& arg) {
  return std::stod(arg);
};

template <typename Arith>
requires std::floating_point<Arith> &&(sizeof(Arith) > 8) Arith
    stoArith(const std::string& arg) {
  return std::stold(arg);
};

namespace Variant {
// metafunction to determine whether a type is a variant
template <typename, typename = void>
struct is_variant : std::false_type {};

template <typename T>
struct is_variant<T, std::void_t<std::variant_alternative_t<0, T>>>
    : std::true_type {};

// compare a single variant alternative with all other alternatives
template <int compare_to, int index, typename var,
          template <typename T, typename U> class compare>
struct single_compare {
  // compare two with a specific variant alternativ, return a bool, 'AND' result
  // with result of next comparison
  static constexpr bool value =
      compare<std::variant_alternative_t<compare_to, var>,
              std::variant_alternative_t<index, var>>::value &&
      single_compare<compare_to, index - 1, var, compare>::value;
};

// ends when index is down to -1
template <int compare_to, typename var,
          template <typename T, typename U> class compare>
struct single_compare<compare_to, -1, var, compare> {
  static constexpr bool value = true;
};

// does not compare alternative to itself
template <int index, typename var,
          template <typename T, typename U> class compare>
struct single_compare<index, index, var, compare> {
  static constexpr bool value =
      true && single_compare<index, index - 1, var, compare>::value;
};

// runs single_compare for all variant alternatives
template <int var_size, int index, typename var,
          template <typename T, typename U> class compare>
struct compare_all {
  // run single comparison for one alternative, return bool, 'AND' result with
  // comparison for next alternative
  static constexpr bool value =
      single_compare<index, var_size, var, compare>::value &&
      compare_all<var_size, index - 1, var, compare>::value;
};

// stop when comparison is done for all alternatives
template <int var_size, typename var,
          template <typename T, typename U> class compare>
struct compare_all<var_size, -1, var, compare> {
  static constexpr bool value = true;
};

// comparison wrapper
template <typename var, template <typename T, typename U> class compare>
struct validate_variant {
  static constexpr bool value =
      compare_all<std::variant_size_v<var> - 1, std::variant_size_v<var> - 1,
                  var, compare>::value;
};

// uses VALIDATE class to validate variant only alternatives specified via
// INDECES
template <typename VARIANT,
          template <typename VARIANT_ALTERNATIVE> class VALIDATE, size_t INDEX,
          size_t... INDECES>
struct validate_variant_at_indeces {
  static constexpr bool value =
      VALIDATE<std::variant_alternative_t<INDEX, VARIANT>>::value &&
      validate_variant_at_indeces<VARIANT, VALIDATE, INDECES...>::value;
};

// specialization for last index
template <typename VARIANT,
          template <typename VARIANT_ALTERNATIVE> class VALIDATE, size_t INDEX>
struct validate_variant_at_indeces<VARIANT, VALIDATE, INDEX> {
  static constexpr bool value =
      VALIDATE<std::variant_alternative_t<INDEX, VARIANT>>::value;
};

// aggregates a specific attribute over all variant alternatives
// retriever class-templates retrieves said attribute (e.g. a static member)
// from variant alternatives aggregator class-template specifies the mode of
// aggregation (e.g. addition or some other arithmetic operation) neutral value
// represents the final value to be aggregated at index -1 (e.g. 0 for addition)
template <typename VARIANT, typename RESULT_TYPE, RESULT_TYPE neutral_val,
          template <RESULT_TYPE R1, RESULT_TYPE R2> class aggregator,
          template <typename VAR_ALTERNATIVE> class retriever,
          int index = std::variant_size_v<VARIANT> - 1>
struct aggregate_over_variant {
  static constexpr RESULT_TYPE value = aggregator<
      retriever<std::variant_alternative_t<index, VARIANT>>::value,
      aggregate_over_variant<VARIANT, RESULT_TYPE, neutral_val, aggregator,
                             retriever, index - 1>::value>::value;
};

template <typename VARIANT, typename RESULT_TYPE, RESULT_TYPE neutral_val,
          template <RESULT_TYPE R1, RESULT_TYPE R2> class aggregator,
          template <typename VAR_ALTERNATIVE> class retriever>
struct aggregate_over_variant<VARIANT, RESULT_TYPE, neutral_val, aggregator,
                              retriever, -1> {
  static constexpr RESULT_TYPE value = neutral_val;
};
};  // namespace Variant
};  // namespace Auxil