#pragma once

#include <cstdint>
#include <span>

namespace FIXmsgTypes{
	struct addLimitOrder{
		public:
			static constexpr std::uint32_t msgLength{17};
			static constexpr std::uint32_t lengthOffset{0};
			static constexpr std::uint32_t headerLength {6};
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
	
	struct withdrawLimitOrder{
		public:
			static constexpr std::uint32_t msgLength{18};
			static constexpr std::uint32_t lengthOffset{0};
			static constexpr std::uint32_t headerLength {6};
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
	
		struct marketOrder{
		public:
			static constexpr std::uint32_t msgLength{13};
			static constexpr std::uint32_t lengthOffset{0};
			static constexpr std::uint32_t headerLength {6};
			static constexpr std::uint32_t delimiterOffset{4};
			static constexpr std::uint32_t delimiterValue{0xEB50};
			static constexpr std::uint32_t volumeOffset{6};
			std::int32_t orderVolume;
			explicit marketOrder(std::int32_t);
			explicit marketOrder(const std::span<std::uint8_t>&);
			explicit marketOrder();
	};
};