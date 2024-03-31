#pragma once

#include <atomic>
#include <concepts>
#include <span>
#include <type_traits>
#include <utility>


namespace SLQ_Auxil {
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


template<typename...>
struct atomic_arr_copy{
  static_assert(false);
};

template<typename T, size_t... indices_8byte, size_t... indices_1byte>
requires std::is_trivially_copyable_v<T>
&& std::is_default_constructible_v<T>
&& std::atomic<std::uint64_t>::is_always_lock_free
struct atomic_arr_copy<T, std::integer_sequence<size_t, indices_8byte...>, std::integer_sequence<size_t, indices_1byte...>>{
private:
  static constexpr size_t size = sizeof(T);
  static constexpr size_t one_byte_threshold = 8 * (size/8);
  T value;
public:
  using type = T;
  T return_instance() const noexcept{
    return value;
  };

  atomic_arr_copy(){};

  atomic_arr_copy(const T arg): value{arg}{};

  atomic_arr_copy(const atomic_arr_copy&) = delete;

  atomic_arr_copy& operator=(const atomic_arr_copy& other){
    // make relevant sections of memory accessible via std::span
    const auto source_span_8b = std::span<const std::atomic<std::uint64_t>>{reinterpret_cast<const std::atomic<std::uint64_t>*>(&other.value), size/8};
    const auto dest_span_8b = std::span<std::atomic<std::uint64_t>>{reinterpret_cast<std::atomic<std::uint64_t>*>(&this->value), size/8};
    const auto source_span_1b = std::span<const std::atomic<char>>{reinterpret_cast<const std::atomic<char>*>(&other.value), size};
    const auto dest_span_1b = std::span<std::atomic<char>>{reinterpret_cast<std::atomic<char>*>(&this->value), size};
    // atomically copy date
    (dest_span_8b[indices_8byte].store(source_span_8b[indices_8byte], std::memory_order_relaxed),...);
    (dest_span_1b[indices_1byte + one_byte_threshold].store(source_span_1b[indices_1byte + one_byte_threshold], std::memory_order_relaxed),...);

    return *this;
  };

  atomic_arr_copy(atomic_arr_copy&&) = delete;

  atomic_arr_copy& operator=(const atomic_arr_copy&& other){
    return operator=(other);
  };
};

template <typename T>
using atomic_arr_copy_t = atomic_arr_copy<T, std::make_index_sequence<sizeof(T)/8>, std::make_index_sequence<sizeof(T) % 8>>;

template <typename T>
requires std::is_default_constructible_v<T>
&& std::is_copy_assignable_v<T>
struct atomic_arr_copy_standin{
private:
  T value;
public:
  using type = T;
  T return_instance() const noexcept{
    return value;
  };

  atomic_arr_copy_standin(){};

  atomic_arr_copy_standin(const T& arg): value(arg){};

  atomic_arr_copy_standin(const atomic_arr_copy_standin&) = delete;

  atomic_arr_copy_standin& operator=(const atomic_arr_copy_standin& other){
    this->value = other.value;
    return *this;
  };

  atomic_arr_copy_standin(atomic_arr_copy_standin&&) = delete;

  atomic_arr_copy_standin& operator=(const atomic_arr_copy_standin&& other){
    return operator=(other);
  };

};

template<typename, bool>
struct UB_or_not_UB_logic{
    static_assert(false);
};

template<typename T>
struct UB_or_not_UB_logic<T, true>{
  using type = atomic_arr_copy_standin<T>;
};

template<typename T>
struct UB_or_not_UB_logic<T, false>{
  using type = atomic_arr_copy_t<T>;
};

template<typename T, bool accept_UB>
using UB_or_not_UB = UB_or_not_UB_logic<T, accept_UB>::type;

};
