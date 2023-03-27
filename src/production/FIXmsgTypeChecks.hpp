#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

#include "Auxil.hpp"

namespace MsgTypeChecks {
template <typename fixMsgObj>
concept FixMsgClass = requires(const std::span<std::uint8_t>& charSpan) {
  { fixMsgObj::msgLength } -> std::same_as<const std::uint32_t&>;
  { fixMsgObj::headerLength } -> std::same_as<const std::uint32_t&>;
  { fixMsgObj::lengthOffset } -> std::same_as<const std::uint32_t&>;
  { fixMsgObj::delimiterOffset } -> std::same_as<const std::uint32_t&>;
  { fixMsgObj::delimiterValue } -> std::same_as<const std::uint32_t&>;
  { fixMsgObj::volumeOffset } -> std::same_as<const std::uint32_t&>;
  { fixMsgObj::fixMsgObj(charSpan) } -> std::same_as<fixMsgObj>;
};

template <typename queueType, typename msgType>
concept fixDestinationQueue = requires(queueType queue, msgType msg,
                                       bool bool_) {
  { queue.enqueue(msg, bool_) } -> std::same_as<bool>;
};

// defines interface for market order class required by order book class
template <typename MarketOrderClass>
concept OrderBookMarketOrderInterfaceConcept =
    requires(MarketOrderClass marketOrder, std::int32_t i32) {
  { marketOrder.orderVolume } -> std::same_as<std::int32_t&>;
  { MarketOrderClass(i32) } -> std::same_as<MarketOrderClass>;
};

// defines interface for limit order class required by order book class
template <typename LimitOrderClass>
concept OrderBookLimitOrderInterfaceConcept =
    requires(LimitOrderClass limitOrder, std::int32_t i32, std::uint32_t ui32) {
  { limitOrder.orderVolume } -> std::same_as<std::int32_t&>;
  { limitOrder.orderPrice } -> std::same_as<std::uint32_t&>;
  { LimitOrderClass(i32, ui32) } -> std::same_as<LimitOrderClass>;
};

// wrapper class to make OrderBookMarketOrderInterface_ concept suitalbe for
// classic TMP
template <typename MarketOrderClass>
struct OrderBookMarketOrderInterfaceMetafunc {
  static constexpr bool value =
      OrderBookMarketOrderInterfaceConcept<MarketOrderClass>;
};

// wrapper class to make OrderBookLimitOrderInterface_ concept suitalbe for
// classic TMP
template <typename LimitOrderClass>
struct OrderBookLimitOrderInterfaceMetafunc {
  static constexpr bool value =
      OrderBookLimitOrderInterfaceConcept<LimitOrderClass>;
};

template <typename MsgClassVariant, size_t... Indeces>
struct OrderBookMarketOrderInterface {
  static constexpr bool value = Auxil::Variant::validate_variant_at_indeces<
      MsgClassVariant, OrderBookMarketOrderInterfaceMetafunc,
      Indeces...>::value;
};

template <typename MsgClassVariant, size_t... Indeces>
struct OrderBookLimitOrderInterface {
  static constexpr bool value = Auxil::Variant::validate_variant_at_indeces<
      MsgClassVariant, OrderBookLimitOrderInterfaceMetafunc, Indeces...>::value;
};

// check to ensure no two messages in variant have the same message length
template <typename FixMsg1, typename FixMsg2>
struct msgLenNotEqual {
  static constexpr bool value = FixMsg1::msgLength != FixMsg2::msgLength;
};

template <typename Variant>
using checkMsgLenNotEqual =
    Auxil::Variant::validate_variant<Variant, msgLenNotEqual>;

// check to ensure postion of message length in byte stream is the same across
// all message types in variant
template <typename FixMsg1, typename FixMsg2>
struct lenOffsetEqual {
  static constexpr bool value = FixMsg1::lengthOffset == FixMsg1::lengthOffset;
};

template <typename Variant>
using checkLenOffsetEqual =
    Auxil::Variant::validate_variant<Variant, lenOffsetEqual>;

// check to ensure postion of header length in byte stream is the same across
// all message types in variant
template <typename FixMsg1, typename FixMsg2>
struct headerLenEqual {
  static constexpr bool value = FixMsg1::headerLength == FixMsg2::headerLength;
};

template <typename Variant>
using checkHeaderLenEqual =
    Auxil::Variant::validate_variant<Variant, headerLenEqual>;

// check to ensure postion of delimiter in byte stream is the same across all
// message types in variant
template <typename FixMsg1, typename FixMsg2>
struct delimOffsetEqual {
  static constexpr bool value =
      FixMsg1::delimiterOffset == FixMsg2::delimiterOffset;
};

template <typename Variant>
using checkDelimOffsetEqual =
    Auxil::Variant::validate_variant<Variant, delimOffsetEqual>;

// check to ensure value of delimiter in byte stream is the same across all
// message types in variant
template <typename FixMsg1, typename FixMsg2>
struct delimLenEqual {
  static constexpr bool value =
      FixMsg1::delimiterLength == FixMsg2::delimiterLength;
};

// check to ensure value of delimiter in byte stream is the same across all
// message types in variant
template <typename FixMsg1, typename FixMsg2>
struct delimValEqual {
  static constexpr bool value =
      FixMsg1::delimiterValue == FixMsg2::delimiterValue;
};

template <typename Variant>
using checkDelimValEqual =
    Auxil::Variant::validate_variant<Variant, delimValEqual>;

template <typename FixMsg1, typename FixMsg2>
struct volOffsetEqual {
  static constexpr bool value = FixMsg1::volumeOffset == FixMsg2::volumeOffset;
};

template <typename Variant>
using checkVolOffsetEqual =
    Auxil::Variant::validate_variant<Variant, volOffsetEqual>;

// combines all the checks specified above
template <typename msgClassVariant>
struct consistencyChecksAcrossMsgTypes {
  static constexpr bool value = checkMsgLenNotEqual<msgClassVariant>::value &&
                                checkLenOffsetEqual<msgClassVariant>::value &&
                                checkHeaderLenEqual<msgClassVariant>::value &&
                                checkDelimOffsetEqual<msgClassVariant>::value &&
                                checkVolOffsetEqual<msgClassVariant>::value &&
                                checkDelimValEqual<msgClassVariant>::value;
};

// determine longest message length across all message types in variant
template <typename FixMsg>
struct retrieveMsgLen {
  static constexpr std::uint32_t value = FixMsg::msgLength;
};

template <std::uint32_t I1, std::uint32_t I2>
struct UInt32Max {
  static constexpr std::uint32_t value = std::max(I1, I2);
};

template <typename Variant>
using determineMaxMsgLen =
    Auxil::Variant::aggregate_over_variant<Variant, std::uint32_t, 0, UInt32Max,
                                           retrieveMsgLen>;

template <std::uint32_t I1, std::uint32_t I2>
struct UInt32Min {
  static constexpr std::uint32_t value = std::min(I1, I2);
};

template <typename Variant>
using determineMinMsgLen = Auxil::Variant::aggregate_over_variant<
    Variant, std::uint32_t, std::numeric_limits<std::uint32_t>::max(),
    UInt32Min, retrieveMsgLen>;

// ensure all message classes are default constructible
template <bool b1, bool b2>
struct andAggregator {
  static constexpr bool value = b1 && b2;
};

template <typename Variant_>
struct alternativesDefaultConstructible {
  static constexpr bool value = Auxil::Variant::aggregate_over_variant<
      Variant_, bool, true, andAggregator,
      std::is_default_constructible>::value;
};
};  // namespace MsgTypeChecks
