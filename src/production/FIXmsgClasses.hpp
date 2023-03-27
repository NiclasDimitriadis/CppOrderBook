#pragma once

#include <cstdint>
#include <span>

namespace FIXmsgClasses {
struct addLimitOrder {
 public:
  static constexpr std::uint32_t msgLength{17};
  static constexpr std::uint32_t lengthOffset{0};
  static constexpr std::uint32_t headerLength{6};
  static constexpr std::uint32_t delimiterOffset{4};
  static constexpr std::uint32_t delimiterValue{0xEB50};
  static constexpr std::uint32_t volumeOffset{6};
  static constexpr std::uint32_t priceOffset{10};
  std::int32_t orderVolume;
  std::uint32_t orderPrice;
  explicit addLimitOrder(std::int32_t, std::uint32_t);
  explicit addLimitOrder(const std::span<std::uint8_t>&);
  explicit addLimitOrder();
};

struct withdrawLimitOrder {
 public:
  static constexpr std::uint32_t msgLength{18};
  static constexpr std::uint32_t lengthOffset{0};
  static constexpr std::uint32_t headerLength{6};
  static constexpr std::uint32_t delimiterOffset{4};
  static constexpr std::uint16_t delimiterlength{2};
  static constexpr std::uint16_t delimiterValue{0xEB50};
  static constexpr std::uint32_t volumeOffset{6};
  static constexpr std::uint32_t priceOffset{10};
  std::int32_t orderVolume;
  std::uint32_t orderPrice;
  explicit withdrawLimitOrder(std::int32_t, std::uint32_t);
  explicit withdrawLimitOrder(const std::span<std::uint8_t>&);
  explicit withdrawLimitOrder();
};

struct marketOrder {
 public:
  static constexpr std::uint32_t msgLength{13};
  static constexpr std::uint32_t lengthOffset{0};
  static constexpr std::uint32_t headerLength{6};
  static constexpr std::uint32_t delimiterOffset{4};
  static constexpr std::uint32_t delimiterValue{0xEB50};
  static constexpr std::uint32_t volumeOffset{6};
  std::int32_t orderVolume;
  explicit marketOrder(std::int32_t);
  explicit marketOrder(const std::span<std::uint8_t>&);
  explicit marketOrder();
};
};  // namespace FIXmsgClasses

namespace FIXmsgClasses {
addLimitOrder::addLimitOrder(const std::span<std::uint8_t>& bufferSpan) {
  this->orderVolume =
      *reinterpret_cast<std::int32_t*>(&bufferSpan[volumeOffset]);
  this->orderPrice =
      *reinterpret_cast<std::uint32_t*>(&bufferSpan[priceOffset]);
};

addLimitOrder::addLimitOrder(std::int32_t orderVolume_,
                             std::uint32_t orderPrice_)
    : orderVolume{orderVolume_}, orderPrice{orderPrice_} {};

addLimitOrder::addLimitOrder() : orderVolume{0}, orderPrice{0} {};

withdrawLimitOrder::withdrawLimitOrder(
    const std::span<std::uint8_t>& bufferSpan) {
  this->orderVolume =
      *reinterpret_cast<std::int32_t*>(&bufferSpan[volumeOffset]);
  this->orderPrice =
      *reinterpret_cast<std::uint32_t*>(&bufferSpan[priceOffset]);
};

withdrawLimitOrder::withdrawLimitOrder(std::int32_t orderVolume_,
                                       std::uint32_t orderPrice_)
    : orderVolume{orderVolume_}, orderPrice{orderPrice_} {};

withdrawLimitOrder::withdrawLimitOrder() : orderVolume{0}, orderPrice{0} {};

marketOrder::marketOrder(const std::span<std::uint8_t>& bufferSpan) {
  this->orderVolume =
      *reinterpret_cast<std::int32_t*>(&bufferSpan[volumeOffset]);
};

marketOrder::marketOrder(std::int32_t orderVolume_)
    : orderVolume{orderVolume_} {};

marketOrder::marketOrder() : orderVolume{0} {};
};  // namespace FIXmsgClasses