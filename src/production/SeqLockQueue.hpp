#pragma once

#include <algorithm>
#include <atomic>
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <span>
#include <type_traits>

#include "Auxil.hpp"
#include "SeqLockElement.hpp"

namespace SeqLockQueue {
template <typename contentType_, std::uint32_t length_, bool shareCacheline>
requires(std::has_single_bit(length_))
 struct seqLockQueue {
 private:
  static constexpr std::uint16_t cacheline = 64;
  static constexpr std::uint32_t length = length_;
  static constexpr std::uint16_t defaultAlignment =
      alignof(SeqLockElement::seqLockElement<contentType_, 0>);
  static constexpr std::uint16_t alignment =
      shareCacheline * Auxil::divisible_or_ceil(defaultAlignment, cacheline) +
      cacheline * !shareCacheline *
          (defaultAlignment / cacheline +
           1 * (defaultAlignment % cacheline != 0));
  using contentType = contentType_;
  using elementType = SeqLockElement::seqLockElement<contentType, alignment>;
  static constexpr std::uint16_t elementSize = sizeof(elementType);
  static constexpr std::uint32_t byteLength =
      std::max(alignment, elementSize) * length;
  void* memoryPointer;
  // data used by dequeueing thread
  alignas(cacheline) std::int64_t dequeueIndex = 0;
  std::int64_t prevVersion = 1;
  std::span<elementType, length> dequeueSpan;
  // data used by enqueueing thread
  alignas(cacheline) std::int64_t enqueueIndex = 0;
  std::span<elementType, length> enqueueSpan;

 public:
  explicit seqLockQueue();
  ~seqLockQueue();
  seqLockQueue(const seqLockQueue&) = delete;
  seqLockQueue& operator=(const seqLockQueue&) = delete;
  seqLockQueue(seqLockQueue&&) = delete;
  seqLockQueue& operator=(seqLockQueue&&) = delete;
  void enqueue(contentType) noexcept;
  std::optional<contentType> dequeue() noexcept;
};
}  // namespace SeqLockQueue

#define TEMPLATE_PARAMS                                                        \
  template <typename contentType_, std::uint32_t length_, bool shareCacheline> \
  requires(std::has_single_bit(length_))

#define SEQ_LOCK_QUEUE \
  SeqLockQueue::seqLockQueue<contentType_, length_, shareCacheline>

TEMPLATE_PARAMS
SEQ_LOCK_QUEUE::seqLockQueue()
    : dequeueSpan{reinterpret_cast<elementType*>(memoryPointer), length},
      enqueueSpan{reinterpret_cast<elementType*>(memoryPointer), length} {
  this->memoryPointer = std::aligned_alloc(cacheline, byteLength);
  this->dequeueSpan = std::span<elementType, length>(
      reinterpret_cast<elementType*>(this->memoryPointer), length);
  this->enqueueSpan = std::span<elementType, length>(
      reinterpret_cast<elementType*>(this->memoryPointer), length);
  std::ranges::fill(this->dequeueSpan, elementType());
  //std::cout << "byte length = " << byteLength << "\n";
};

TEMPLATE_PARAMS
SEQ_LOCK_QUEUE::~seqLockQueue() { std::free(this->memoryPointer); };

TEMPLATE_PARAMS
std::optional<contentType_> SEQ_LOCK_QUEUE::dequeue() noexcept {
  auto readResult =
      this->dequeueSpan[this->dequeueIndex % length].read(this->prevVersion);
  bool entryRead = std::get<0>(readResult).has_value();
  this->dequeueIndex += 1 * entryRead;
  this->prevVersion = entryRead ? std::get<1>(readResult) : this->prevVersion;
  return std::get<0>(readResult);
};

TEMPLATE_PARAMS
void SEQ_LOCK_QUEUE::enqueue(contentType_ content_) noexcept {
  this->enqueueSpan[this->enqueueIndex % length].insert(content_);
  ++this->enqueueIndex;
};

#undef TEMPLATE_PARAMS
#undef SEQ_LOCK_QUEUE