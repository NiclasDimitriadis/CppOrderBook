#include <cstdint>
#include <span>

#include "FIXmsgClasses.hpp"

namespace FIXmsgTypes{
	addLimitOrder::addLimitOrder(const std::span<std::uint8_t>& bufferSpan){
		this->orderVolume = *reinterpret_cast<std::int32_t*>(&bufferSpan[volumeOffset]);
		this->orderPrice = *reinterpret_cast<std::uint32_t*>(&bufferSpan[priceOffset]);
	};
	
	addLimitOrder::addLimitOrder(std::int32_t orderVolume_, std::uint32_t orderPrice_): orderVolume{orderVolume_}, orderPrice{orderPrice_};
	
	addLimitOrder::addLimitOrder(): orderVolume{0}, orderPrice{0};
	
	
	
	withdrawLimitOrder::withdrawLimitOrder(const std::span<std::uint8_t>& bufferSpan){
		this->orderVolume = *reinterpret_cast<std::int32_t*>(&bufferSpan[volumeOffset]);
		this->orderPrice = *reinterpret_cast<std::uint32_t*>(&bufferSpan[priceOffset]);
	};
	
	withdrawLimitOrder::withdrawLimitOrder(std::int32_t orderVolume_, std::uint32_t orderPrice_): orderVolume{orderVolume_}, orderPrice{orderPrice_};
	
	withdrawLimitOrder::withdrawLimitOrder(): orderVolume{0}, orderPrice{0};
	
	
	
	marketOrder::marketOrder(const std::span<std::uint8_t>& bufferSpan){
		this->orderVolume = *reinterpret_cast<std::int32_t*>(&bufferSpan[volumeOffset]);
	};
	
	marketOrder::marketOrder(std::int32_t orderVolume_): orderVolume{orderVolume_};
	
	marketOrder::marketOrder(): orderVolume{0};
};