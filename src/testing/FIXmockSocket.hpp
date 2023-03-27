#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <numeric>
#include <span>
#include <tuple>
#include <vector>

namespace FIXmockSocket {
struct fixMockSocket {
 private:
  // calculates and inserts FIX checksum
  void insertChecksum(std::uint32_t, std::uint32_t) noexcept;
  // functions to insert byte patterns for specific messages into memory, to be
  // called only during construction
  void insertNewLimitOrder(
      const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>&,
      std::uint32_t);
  void insertWithdrawLimitOrder(
      const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>&,
      std::uint32_t);
  void insertMarketOrder(
      const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>&,
      std::uint32_t);
  void insertOversizedMsg(
      const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>&,
      std::uint32_t);
  std::uint32_t memSize;
  std::uint8_t* memPtr;
  std::span<uint8_t> memSpan;
  // iterates over vector, retreives message date from tuple, calls respective
  // insert function to be called only during construction
  void populateMemory(const std::vector<std::tuple<std::uint8_t, std::int32_t,
                                                   std::uint32_t>>&) noexcept;
  // iterates over vector, determines numer of bytes needed to store all
  // messages to be called only during construction
  std::uint32_t determineMemSize(
      const std::vector<
          std::tuple<std::uint8_t, std::int32_t, std::uint32_t>>&);
  // bookkeeping on number of messages already read and index for next read
  std::uint32_t readIndex = 0;
  // flag to set when end of the buffer is reached
  std::atomic_flag* endOfBuffer;

 public:
  // construct mock socket from vector of tuples containing:
  // unit8_t: message type(0: newLimitOrder, 1 withdrawLimitOrder, 2
  // marketOrder) int32_t order volume uint32_t order price
  fixMockSocket(
      const std::vector<std::tuple<std::uint8_t, std::int32_t, std::uint32_t>>&,
      std::atomic_flag*);
  ~fixMockSocket();
  std::int32_t recv(void*, std::uint32_t) noexcept;
};
};  // namespace FIXmockSocket

namespace FIXmockSocket {
void fixMockSocket::insertChecksum(std::uint32_t msgIndex,
                                   std::uint32_t checksumIndex) noexcept {
  auto accumSpan = this->memSpan.subspan(msgIndex, checksumIndex - msgIndex);
  const std::uint8_t byteSum =
      std::accumulate(accumSpan.begin(), accumSpan.end(), 0);
  this->memSpan[checksumIndex] = (byteSum / 100) + 48;
  this->memSpan[checksumIndex + 1] = ((byteSum % 100) / 10) + 48;
  this->memSpan[checksumIndex + 2] = (byteSum % 10) + 48;
};

void fixMockSocket::insertNewLimitOrder(
    const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>& orderTuple,
    std::uint32_t beginIndex) {
  *reinterpret_cast<std::uint32_t*>(&this->memSpan[beginIndex]) = 17;
  *reinterpret_cast<std::uint16_t*>(&this->memSpan[beginIndex + 4]) = 0xEB50;
  *reinterpret_cast<std::int32_t*>(&this->memSpan[beginIndex + 6]) =
      std::get<1>(orderTuple);
  *reinterpret_cast<std::uint32_t*>(&this->memSpan[beginIndex + 10]) =
      std::get<2>(orderTuple);
  this->insertChecksum(beginIndex, beginIndex + 14);
};

void fixMockSocket::insertWithdrawLimitOrder(
    const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>& orderTuple,
    std::uint32_t beginIndex) {
  *reinterpret_cast<std::uint32_t*>(&this->memSpan[beginIndex]) = 18;
  *reinterpret_cast<std::uint16_t*>(&this->memSpan[beginIndex + 4]) = 0xEB50;
  *reinterpret_cast<std::int32_t*>(&this->memSpan[beginIndex + 6]) =
      std::get<1>(orderTuple);
  *reinterpret_cast<std::uint32_t*>(&this->memSpan[beginIndex + 10]) =
      std::get<2>(orderTuple);
  memSpan[beginIndex + 14] = 0xFF;
  this->insertChecksum(beginIndex, beginIndex + 15);
};

void fixMockSocket::insertMarketOrder(
    const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>& orderTuple,
    std::uint32_t beginIndex) {
  *reinterpret_cast<std::uint32_t*>(&this->memSpan[beginIndex]) = 13;
  *reinterpret_cast<std::uint16_t*>(&this->memSpan[beginIndex + 4]) = 0xEB50;
  *reinterpret_cast<std::int32_t*>(&this->memSpan[beginIndex + 6]) =
      std::get<1>(orderTuple);
  this->insertChecksum(beginIndex, beginIndex + 10);
};

void fixMockSocket::insertOversizedMsg(
    const std::tuple<std::uint8_t, std::int32_t, std::uint32_t>& orderTuple,
    std::uint32_t beginIndex) {
  *reinterpret_cast<std::uint32_t*>(&this->memSpan[beginIndex]) = 40;
  *reinterpret_cast<std::uint16_t*>(&this->memSpan[beginIndex + 4]) = 0xEB50;
  // set 8 bits to make sure checksum will invalidate message even if all bits
  // were zeroed beforehand
  *reinterpret_cast<std::uint8_t*>(&this->memSpan[beginIndex + 6]) = 0xFF;
};

std::uint32_t fixMockSocket::determineMemSize(
    const std::vector<std::tuple<std::uint8_t, std::int32_t, std::uint32_t>>&
        msgVec) {
  std::uint32_t ret{0};
  for (auto msg : msgVec) {
    ret += 17 * (std::get<0>(msg) == 0) + 18 * (std::get<0>(msg) == 1) +
           13 * (std::get<0>(msg) == 2) + 20 * (std::get<0>(msg) == 3) +
           40 * (std::get<0>(msg) == 4);
  };
  return ret;
};

void fixMockSocket::populateMemory(
    const std::vector<std::tuple<std::uint8_t, std::int32_t, std::uint32_t>>&
        msgVec) noexcept {
  std::uint32_t index{0};
  std::uint8_t msgTypeIndex{0};
  for (auto msg : msgVec) {
    msgTypeIndex = std::get<0>(msg);
    switch (msgTypeIndex) {
      case 0:
        this->insertNewLimitOrder(msg, index);
        index += 17;
        break;
      case 1:
        this->insertWithdrawLimitOrder(msg, index);
        index += 18;
        break;
      case 2:
        this->insertMarketOrder(msg, index);
        index += 13;
        break;
      // 20 arbitrary bytes to simulate faulty order
      case 3:
        index += 20;
        break;
      case 4:
        // for checking whether unknown msg types/insignificant messages with
        // valid header can be correctly discarded
        this->insertOversizedMsg(msg, index);
        index += 40;
        break;
    };
  };
};

fixMockSocket::fixMockSocket(
    const std::vector<std::tuple<std::uint8_t, std::int32_t, std::uint32_t>>&
        msgVec,
    std::atomic_flag* flagPtr = nullptr)
    : endOfBuffer(flagPtr) {
  this->memSize = this->determineMemSize(msgVec);
  this->memPtr = new std::uint8_t[this->memSize];
  this->memSpan = std::span<std::uint8_t>{this->memPtr, this->memSize};
  this->populateMemory(msgVec);
};

fixMockSocket::~fixMockSocket() { delete[] this->memPtr; };

std::int32_t fixMockSocket::recv(void* dest, std::uint32_t count) noexcept {
  if ((this->readIndex + count) > this->memSize) {
    std::cout << "fixMockSocket: out of bounds read during call of recv(). "
                 "terminating."
              << "\n";
    std::terminate();
  };
  std::memcpy(dest, &this->memSpan[this->readIndex], count);
  this->readIndex += count;
  if (this->readIndex == this->memSize && this->endOfBuffer != nullptr) {
    this->endOfBuffer->test_and_set(std::memory_order_release);
  };
  return count;
};
}  // namespace FIXmockSocket