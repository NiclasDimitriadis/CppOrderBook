#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <optional>
#include <tuple>
#include <type_traits>

namespace SeqLockElement {
template <typename contentType_, std::uint32_t alignment>
requires std::is_default_constructible_v<contentType_> &&
    std::is_trivially_copyable_v<contentType_>
struct alignas(alignment) seqLockElement {
  using contentType = contentType_;
  contentType content;
  std::int64_t version = 0;
  void insert(contentType) noexcept;
  std::tuple<std::optional<contentType_>, std::int64_t> read(
      std::int64_t) const noexcept;
};
};  // namespace SeqLockElement

#define TEMPLATE_PARAMS                                     \
  template <typename contentType_, std::uint32_t alignment> \
  requires std::is_default_constructible_v<contentType_> && \
      std::is_trivially_copyable_v<contentType_>
#define SEQ_LOCK_ELEMENT SeqLockElement::seqLockElement<contentType_, alignment>

TEMPLATE_PARAMS
void SEQ_LOCK_ELEMENT::insert(contentType_ newContent) noexcept {
  ++this->version;
  // prevents compiler from reordering version-increment and writing new content
  std::atomic_signal_fence(std::memory_order_acq_rel);
  this->content = newContent;
  std::atomic_signal_fence(std::memory_order_acq_rel);
  ++this->version;
};

TEMPLATE_PARAMS
std::tuple<std::optional<contentType_>, std::int64_t> SEQ_LOCK_ELEMENT::read(
  std::int64_t prevVersion) const noexcept {
  std::int64_t initialVersion;
  contentType retContent;
  do {
    initialVersion = this->version;
    std::atomic_signal_fence(std::memory_order_acq_rel);
    retContent = this->content;
    std::atomic_signal_fence(std::memory_order_acq_rel);
  } while (initialVersion % 1 || (initialVersion != this->version));
  std::optional<contentType_> retOpt = initialVersion >= prevVersion
                                           ? std::make_optional(retContent)
                                           : std::nullopt;
  return std::make_tuple(retOpt, initialVersion);
};

#undef TEMPLATE_PARAMS
#undef SEQ_LOCK_ELEMENT