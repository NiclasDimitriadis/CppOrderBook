#pragma once

#include <concepts>
#include <cstdint>

#include "Auxil.hpp"

namespace OrderBookBucket {
template <typename EntryType, std::uint32_t alignment>
requires std::signed_integral<EntryType>
struct alignas(alignment) orderBookBucket {
 private:
  EntryType volume = 0;

 public:
  EntryType consumeLiquidity(EntryType) noexcept;
  void addLiquidity(EntryType) noexcept;
  EntryType getVolume() noexcept;
};
};  // namespace OrderBookBucket

namespace OrderBookBucket {
template <typename EntryType, std::uint32_t alignment>
EntryType orderBookBucket<EntryType, alignment>::consumeLiquidity(
    EntryType fillVolume) noexcept {
  const bool demandLiquidity = Auxil::is_negative(this->volume);
  const bool withdrawDemand = Auxil::is_negative(fillVolume);
  const bool liquidityMatch = demandLiquidity == withdrawDemand;
  const bool sufficientLiquidity =
      demandLiquidity && (this->volume <= fillVolume) ||
      !demandLiquidity && (volume >= fillVolume);
  const EntryType volumeFilled =
      0 + liquidityMatch * (fillVolume * sufficientLiquidity +
                            !sufficientLiquidity * this->volume);
  this->volume -= volumeFilled;
  return volumeFilled;
};

template <typename EntryType, std::uint32_t alignment>
void orderBookBucket<EntryType, alignment>::addLiquidity(
    EntryType addVolume) noexcept {
  this->volume += addVolume;
};

template <typename EntryType, std::uint32_t alignment>
EntryType orderBookBucket<EntryType, alignment>::getVolume() noexcept {
  return this->volume;
};
};  // namespace OrderBookBucket