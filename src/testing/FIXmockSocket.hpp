#pragma once

#include <algorithm>
#include <exception>
#include <cstdint>
#include <cstring>
#include <iostring>
#include <functional>
#include <span>
#include <vector>

namespace FIXMockSocket{
	struct fixMockSocket{
	private:
		//calculates and inserts FIX checksum 
		void insertChecksum(std::uint32_t, std::uint32_t) noexcept;
		//functions to insert byte patterns for specific messages into memory, to be called only during construction
		void insertNewLimitOrder(const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>&, std::uint32_t);
		void insertWithdrawLimitOrder(const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>&, std::uint32_t);
		void insertMarketOrder(const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>&, std::uint32_t);
		std::uint32_t nMsgs;
		std::uint32_t memSize;
		std::uint8_t* memPtr;
		std::span<uint8_t> memSpan;
		//iterates over vector, retreives message date from tuple, calls respective insert function
		//to be called only during construction
		void populateMemory(const std::vector<std::tuple<std::uint8_t, std::int32_t, std::uint32_t>>&) const noexcept;
		//iterates over vector, determines numer of bytes needed to store all messages
		//to be called only during construction
		std::uint32_t determineMemSize(const std::vector<std::tuple<std::uint8_t, std::int32_t, std::uint32_t>>&);
		//bookkeeping on number of messages already read and index for next read
		std::uint64_t msgsRead = 0;
		std::uint32_t readIndex;
	public:
		//construct mock socket from vector of tuples containing:
		//unit8_t: message type(0: newLimitOrder, 1 withdrawLimitOrder, 2 marketOrder)
		//int32_t order volume
		//uint32_t order price
		fixMockSocket(const std::vector<std::tuple<std::uint8_t, std::int32_t, std::uint32_t>>&);
		~fixMockSocket();
		std::uint32_t recv(void*, std::uint32_t) const noexcept;
	};
};