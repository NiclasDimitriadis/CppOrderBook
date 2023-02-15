#include <algorithm>
#include <exception>
#include <cstdint>
#include <cstring>
#include <iostring>
#include <functional>
#include <span>
#include <vector>

#include "FIXmockSocket.hpp"

void FIXMockSocket::fixMockSocket::insertChecksum(std::uint32_t msgIndex, std::uint32_t checksumIndex) noexcept{
	const std::uint8_t byteSum =
			std::ranges::fold_left(this->memSpan.subspan(msgIndex, checksumIndex - msgIndex), 0, std::plus<std::uint8_t>);
	this->memSpan[checksumIndex] = (byteSum/100) + 48;
	this->memSpan[checksumIndex + 1] = ((byteSum%100)/10) + 48;
	this->memSpan[checksumIndex + 2] = (byteSum%10) + 48;
};

void FIXMockSocket::fixMockSocket::insertNewLimitOrder(const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>& orderTuple, std::uint32_t beginIndex){
	*reinterpret_cast<std::uint32_t*>(&this->memSpan[beginIndex]) = 17;
	*reinterpret_cast<std::uint16_t*>(&this->memSpan[beginIndex + 4]) = 0xEB50;
	*reinterpret_cast<std::int32_t*>(&this->memSpan[beginIndex + 6]) = std::get<1>(orderTuple);
	*reinterpret_cast<std::uint32_t*>(&this->memSpan[beginIndex + 10]) = std::get<2>(orderTuple);
	this->insertChecksum(beginIndex, beginIndex + 14);
};

void FIXMockSocket::fixMockSocket::insertWithdrawLimitOrder(const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>& orderTuple, std::uint32_t beginIndex){
	*reinterpret_cast<std::uint32_t*>(&this->memSpan[beginIndex]) = 18;
	*reinterpret_cast<std::uint16_t*>(&this->memSpan[beginIndex + 4]) = 0xEB50;
	*reinterpret_cast<std::int32_t*>(&this->memSpan[beginIndex + 6]) = std::get<1>(orderTuple);
	*reinterpret_cast<std::uint32_t*>(&this->memSpan[beginIndex + 10]) = std::get<2>(orderTuple);
	memSpan[beginIndex + 14] = 0xFF;
	this->insertChecksum(beginIndex, beginIndex + 15);
};

void FIXMockSocket::fixMockSocket::insertMarketOrder(const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>& orderTuple, std::uint32_t beginIndex){
	*reinterpret_cast<std::uint32_t*>(&this->memSpan[beginIndex]) = 13;
	*reinterpret_cast<std::uint16_t*>(&this->memSpan[beginIndex + 4]) = 0xEB50;
	*reinterpret_cast<std::int32_t*>(&this->memSpan[beginIndex + 6]) = std::get<1>(orderTuple);
	this->insertChecksum(beginIndex, beginIndex + 10);
};

void FIXMockSocket::fixMockSocket::determineMemSize(const std::vector<std::tuple<std::uint8_t, std::int32_t, std::uint32_t>>& msgVec){
	std::uint32_t ret{0};
	for(auto msg : msgVec){
		ret += 17*(std::get<0>(msg) == 0) 
				+ 18*(std::get<0>(msg) == 1) 
				+ 13*(std::get<0>(msg) == 2) ;
	};
	return ret;
};

void FIXMockSocket::fixMockSocket::populateMemory(const std::vector<std::tuple<std::uint8_t, std::int32_t, std::uint32_t>>&) const noexcept{
	std::uint32_t index{0};
	for(auto msg : msgVec){
		switch(std::get<0>(msg)){
			case 0:
				this->insertNewLimitOrder(msg, index);
				index += 17;
			case 1:
				this->insertWithdrawLimitOrder(msg, index);
				index += 18;
			case 3:
				this->insertMarketOrder(msg, index);
				index += 13;
		};
};

FIXMockSocket::fixMockSocket(const std::vector<std::tuple<std::uint8_t, std::int32_t, std::uint32_t>>& msgVec){
	this->nMsgs = msgVec.size();
	this->memSize = this->determineMemSize(msgVec);
	this->memPtr = new std::uint8_t[this->memSize];
	this->memSpan = std::span<std::uint8_t>{this->memPtr, this->memSize};
	this->populateMemory(msgVec);
};

FIXMockSocket::~fixMockSocket(){
	delete[] this->memPtr;
};

void FIXMockSocket::fixMockSocket::recv(void* dest, std::size_t count){
	if((this->readIndex + count) > this->memSize
		std::cout << "fixMockSocket: out of bounds read during call of recv(). terminating." << "\n";
		std::terminate();
	std::memcpy(dest, &this->memSpan[this->readIndex], count);
	++this->msgsRead;
	this->readIndex = (this->msgsRead % this->nMsgs) == 0 ? 0 : this->readIndex + count;
};