#pragma once

#include <atomics>
#include <concepts>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <tuple>

namespace SeqLockElement{
	template<typename contentType_, std::uint32_t alignment>
	requires std::is_default_constructible_v<content_type> && std::is_trivially_copyable_v<contentType>
	alignas(alignment) struct seqLockElement{
		using contentType = contentType_;
		contentType content;
		std::int64_t version = 0;
		void insert(contentType) noexcept;
		std::tuple<std::optional<contentType>, std::int64_t> read(std::int64_t prevVersion) const noexcept;
	};
};

#define TEMPLATE_PARAMS template<typename contentType, std::uint32_t alignment> 
#define SEQ_LOCK_ELEMENT SeqLockElement::seqLockElement<contentType, alignment>

TEMPLATE_PARAMS
void SEQ_LOCK_ELEMENT::insert(contentType content_) noexcept{
	++this->version;
	//prevents compiler from reordering version-increment and writing new content
	std::atomic_signal_fence(std::memory_order_acquire_release);
	this->content = content;
	std::atomic_signal_fence(std::memory_order_acquire_release);
	++this->version;
};

TEMPLATE_PARAMS
std::tuple<std::optional<SEQ_LOCK_ELEMENT::contentType>, std::int64_t> SEQ_LOCK_ELEMENT::read(std::uint64_t prevVersion) const noexcept{
	auto initialVersion;
	contentType retContent;
	do{
		initialVersion = this->version;
		std::atomic_signal_fence(std::memory_order_acquire_release);
		retContent = this->content;
		std::atomic_signal_fence(std::memory_order_acquire_release);
	}
	while(initialVersion % 1|| (initialVersion != this->version));
	std::optional<contentType> retOpt =  intialVersion >= prevVersion ? retContent : std::nullopt;
	return std::make_tuple(retOpt, initialVersion);
};

#undefe TEMPLATE_PARAMS
#undef SEQ_LOCK_ELEMENT