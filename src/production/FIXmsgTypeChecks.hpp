#pragma once

#include "Auxil.hpp"

#include <algorithm>
#include <cstdint>

namespace MsgTypeChecks{
	template <typename fixMsgObj>
    concept FixMsgClass = 
    requires (const std::span<std::uint8_t>& charSpan) {
        { fixMsgObj::msgLength} -> std::same_as<const std::uint32_t&>;
        { fixMsgObj::headerLength} -> std::same_as<const std::uint32_t&>;
        { fixMsgObj::lengthPosition} -> std::same_as<const std::uint32_t&>;
        { fixMsgObj::delimiterPosition} -> std::same_as<const std::uint32_t&>;
        { fixMsgObj::delimiterValue} -> std::same_as<const std::uint32_t&>;
        { fixMsgObj::fixMsgObj(charSpan) } -> std::same_as<fixMsgObj>;
    };
	
	template<typename queueType, typename msgType>
	concept fixDestinationQueue =
	requires (queueType queue, msgType msg) {
		{queue.enqueue(msg, bool)} -> std::same_as<bool>;
	};
	
	//defines interface for market order class required by order book class
	template <typename MarketOrderClass>
    concept OrderBookMarketOrderInterfaceConcept =
	requires (MarketOrderClass marketOrder) {
		{marketOrder.volume} -> std::same_as<std::int32_t>;
		{MarketOrderClass::MarketOrderClass(std::int32_t) } -> std::same_as<MarketOrderClass>;
	};
	
	//defines interface for limit order class required by order book class
	template <typename LimitOrderClass>
    concept OrderBookLimitOrderInterfaceConcept =
	requires (fixMsgObj limitOrder) {
		{limitOrder.volume} -> std::same_as<std::int32_t>;
		{limitOrder.price} -> std::same_as<std::uint32_t>;
		{LimitOrderClass::LimitOrderClass(std::int32_t, std::uint32_t) } -> std::same_as<LimitOrderClass>;
	};
	
	//wrapper class to make OrderBookMarketOrderInterface_ concept suitalbe for classic TMP
	template<typename MarketOrderClass>
	struct OrderBookMarketOrderInterfaceMetafunc{
		static constexpr bool value = OrderBookMarketOrderInterfaceConcept<MarketOrderClass>;
	};
	
	//wrapper class to make OrderBookLimitOrderInterface_ concept suitalbe for classic TMP
	template<typename LimitOrderClass>
	struct OrderBookLimitOrderInterfaceMetafunc{
		static constexpr bool value = OrderBookLimitOrderInterfaceConcept<LimitOrderClass>;
	};
	
	template<typename MsgClassVariant, size_t... Indeces>
	struct OrderBookMarketOrderInterface{
		static constexpr bool value = Auxil::validate_variant_at_indeces<MsgClassVariant, OrderBookMarketOrderMetafunc, Indeces>::value;
	};
	
	template<typename MsgClassVariant, size_t... Indeces>
	struct OrderBookLimitOrderInterface{
		static constexpr bool value = Auxil::validate_variant_at_indeces<MsgClassVariant, OrderBookLimitOrderMetafunc, Indeces>::value;
	};
	
	//check to ensure no two messages in variant have the same message length
	template<typename FixMsg1, typename FixMsg2>
	struct msgLenNotEqual{
		static constexpr bool value = FixMsg1::msgLength != FixMsg1::msgLength;
	};
	
	template<typename Variant>
	using struct checkMsgLenNotEqual = Auxil::Variant::validateVariant<Variant, msgLenNotEqual>;
	
	
	//check to ensure postion of message length in byte stream is the same across all message types in variant
	template<typename FixMsg1, typename FixMsg2>
	struct lenPosEqual{
		static constexpr bool value = FixMsg1::lengthPosition == FixMsg1::lengthPosition;
	};
	
	template<typename Variant>
	using struct checkLenPosEqual = Auxil::Variant::validateVariant<Variant, lenPosEqual>;
	
	
	//check to ensure postion of header length in byte stream is the same across all message types in variant
	template<typename FixMsg1, typename FixMsg2>
	struct headerLenEqual{
		static constexpr bool value = FixMsg1::headerLength == FixMsg2::headerLength;
	};
	
	template<typename Variant>
	using struct checkHeaderLenEqual = Auxil::Variant::validateVariant<Variant, headerLenEqual>;
	
	
	//check to ensure postion of delimiter in byte stream is the same across all message types in variant
	template<typename FixMsg1, typename FixMsg2>
	struct delimPosEqual{
		static constexpr bool value = FixMsg1::delimiterPosition == FixMsg2::delimiterPosition;
	};
	
	template<typename Variant>
	using struct checkDelimPosEqual = Auxil::Variant::validateVariant<Variant, delimPosEqual>;
	
	
	//check to ensure value of delimiter in byte stream is the same across all message types in variant
	template<typename FixMsg1, typename FixMsg2>
	struct delimLenEqual{
		static constexpr bool value = FixMsg1::delimiterLength == FixMsg2::delimiterLength;
	};
	
	template<typename Variant>
	using struct checkDelimValEqual = Auxil::Variant::validateVariant<Variant, delimValEqual>;
	
	
	//check to ensure value of delimiter in byte stream is the same across all message types in variant
	template<typename FixMsg1, typename FixMsg2>
	struct delimValEqual{
		static constexpr bool value = FixMsg1::delimiterValue == FixMsg2::delimiterValue;
	};
	
	template<typename Variant>
	using struct checkDelimValEqual = Auxil::Variant::validateVariant<Variant, delimValEqual>;
	
	
	//combines all the checks specified above
	template<typename msgClassVariant>
	struct consistencyChecksAcrossMsgTypes{
		static constexpr bool value = 
			checkMsgLenNotEqual<msgClassVariant>::value && checkLenPosEqual<msgClassVariant>::value
			&& checkHeaderLenEqual<msgClassVariant>::value && checkDelimPosEqual<msgClassVariant>::value
			&& checkDelimLenEqual<msgClassVariant>::value && delimValEqual<msgClassVariant>::value;
	};
	
	
	//determine longes message length across all message types in variant
	template<typename FixMsg>
	struct retrieveMsgLen{
		static constexpr std::uint32_t value = FixMsg::msgLength;
	};
	
	template<std::uint32_t I1, std::uint32_t I2>
	struct UInt32Max{
		static constexpr std::uint32_t value = std::max(I1, I2);
	};
	
	template<typename Variant>
	using determineMaxMsgLen = Auxil::Variant::aggregate_over_variant<Variant, std::uint32_t, 0, UInt32Max, retrieveMsgLen>;
	
	
	//ensure all message classes are default constructible
	template<bool b1, bool b2>
	struct andAggregator{
		static constexpr bool value = b1 && b2;
	};
	
	template<typename Variant_>
	struct alternativesDefaultConstructible{
		static constexpr bool value =
			Auxil::aggregate_over_variant<Variant_, bool, true, andAggregator, std::is_default_constructible>::value;
	};
};

