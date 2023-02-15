#pragma once

#include <algorithm>
#include <atomics>
#include <bit>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <type_traits>

#include "Auxil.hpp"
#include "SeqLockElement.hpp"

namespace SeqLockQueue{
	template<typename contentType_, std::uint32_t length_,  bool shareCacheline = true>
	requires std::has_single_bit(length)
	struct seqLockQueue{
	private:
		static constexpr std::uint32_t length = length_;
		static constexpr std::uint16_t defaultAlignment = alignof(SeqLockElement::seqLockElement<contentType_, 0>); 
		static constexpr std::uint16_t alignment = shareCacheline*Auxil::divisible_or_ceil(defaultAlingment, 64) 
			+ 64*!shareCacheline*(defaultAlignment/64 + 1*(defaultAlignment%64 != 0));
		using contentType = contentType_;
		using elementType = seqLockElement<contentType, alignment>;
		static constexpr std::uint32_t byteLength = alignment * length;
		void* memoryPointer;
		//data used by dequeueing thread
		alignas(64) std::int64_t dequeueIndex = 0;
		std::int64_t prevVersion = 1;
		std::span<elementType, length> dequeueSpan; 
		//data used by enqueueing thread
		alignas(64) std::int64_t enqueueIndex = 0;
		std::span<elementType, length> enqueueSpan;
	public:
		explicit SeqLockQueue();
		~SeqLockQueue();
		SeqLockQueue(const SeqLockQueue&) = delete;
		SeqLockQueue& operator=(const SeqLockQueue&) = delete;
		SeqLockQueue(SeqLockQueue&&) = delete;
		SeqLockQueue& operator=(SeqLockQueue&&) = delete;
		void enqueue(contentType) noexcept;
		std::optional<contentType> dequeue() noexcept;
	};
}

#define TEMPLATE_PARAMS template<typename contentType_, std::uint32_t length_,  bool shareCacheline = true>
#define SEQ_LOCK_QUEUE SeqLockQueue::seqLockQueue<contentType_, length_, shareCacheline>


TEMPLATE_PARAMS
SEQ_LOCK_QUEUE::seqLockQueue() noexcept{
	this->memoryPointer = std::aligned_alloc(alignment, this->byteLength);
	this->dequeueSpan = std::span<elementType>{(elementType*)this->memoryPointer, length};
	this->enqueueSpan = this->dequeueSpan;
	std::ranges::fill(dequeueSpan,entryType());
};

TEMPLATE_PARAMS
SEQ_LOCK_QUEUE::~seqLockQueue() noexcept{
	std::free(this->memoryPointer);
};

TEMPLATE_PARAMS
std::optional<SEQ_LOCK_QUEUE::contentType> SEQ_LOCK_QUEUE::dequeue() noexcept{
	auto readResult = this->dequeueSpan[this->dequeueIndex%length].read(this->prevVersion);
	bool entryRead = std::get<0>(readResult).has_value();
	this->dequeueIndex += 1*entryRead;
	this->prevVersion = entryRead ? std::get<1>(readResult) : this->prevVersion;
	return std::get<0>(readResult);
};

TEMPLATE_PARAMS
void SEQ_LOCK_QUEUE::enqueue(contentType content_) noexcept{
	this->enqueueSpan[this->enqueueIndex%length].insert(content_);
	++this->enqueueIndex;
};


#undef TEMPLATE_PARAMS template<typename contentType_, std::uint32_t length_,  bool shareCacheline = true>
#undef SEQ_LOCK_QUEUE SeqLockQueue::seqLockQueue<contentType_, length_, shareCacheline>