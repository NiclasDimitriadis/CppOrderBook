#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <new>
#include <optional>
#include <span>

#include "SLQ_Auxil.hpp"
#include "Element.hpp"

namespace Queue {
template <typename ContentType_, std::uint32_t length_, bool share_cacheline, bool accept_UB>
  requires(std::has_single_bit(length_)) &&
  std::is_default_constructible_v<ContentType_> &&
  std::is_trivially_copyable_v<ContentType_> &&
  (!std::is_const_v<ContentType_>)&&
  std::atomic<std::int64_t>::is_always_lock_free
struct SeqLockQueue {
private:
  static constexpr std::uint16_t cacheline = 64;
  static constexpr std::uint32_t length = length_;
  static constexpr std::uint16_t default_alignment =
      alignof(Element::SeqLockElement<SLQ_Auxil::UB_or_not_UB<ContentType_, accept_UB>, 0>);
  static constexpr std::uint16_t alignment =
      share_cacheline * SLQ_Auxil::divisible_or_ceil(default_alignment, cacheline) +
      cacheline * !share_cacheline *
          (default_alignment / cacheline +
           1 * (default_alignment % cacheline != 0));

  using ElementType = Element::SeqLockElement<SLQ_Auxil::UB_or_not_UB<ContentType_, accept_UB>, alignment>;
  using ReadReturnType = std::tuple<std::optional<ContentType_>, std::int64_t>;
  static constexpr std::uint16_t element_size = sizeof(ElementType);
  static constexpr std::uint32_t byte_length =
      std::max(alignment, element_size) * length;
  ElementType* const memory_pointer;
  // data used by dequeueing thread
  const std::span<ElementType, length> dequeue_span;
  // data used by enqueueing thread
  alignas(cacheline) std::int64_t enqueue_index = 0;
  const std::span<ElementType, length> enqueue_span;

  struct QueueReader{
  private:
    const SeqLockQueue* queue_ptr;
    std::int64_t read_index = 0;
    std::int64_t prev_version = 1;
  public:
    explicit QueueReader(const SeqLockQueue*) noexcept;
    ~QueueReader() = default;
    QueueReader(const QueueReader &) = delete;
    QueueReader &operator=(const QueueReader &) = delete;
    QueueReader(QueueReader &&) = delete;
    QueueReader &operator=(QueueReader &&) = delete;
    std::optional<ContentType_> read_next_entry() noexcept;
  };

public:
  using ContentType = ContentType_;
  explicit SeqLockQueue();
  ~SeqLockQueue();
  SeqLockQueue(const SeqLockQueue &) = delete;
  SeqLockQueue &operator=(const SeqLockQueue &) = delete;
  SeqLockQueue(SeqLockQueue &&) = delete;
  SeqLockQueue &operator=(SeqLockQueue &&) = delete;
  using ReaderType = QueueReader;
  void enqueue(const ContentType) noexcept;
  ReadReturnType read_element(std::int64_t, std::int64_t) const noexcept;
  QueueReader get_reader() const noexcept;
};
} // namespace SeqLockQueue

#define TEMPLATE_PARAMS                                                        \
  template <typename ContentType_, std::uint32_t length_, bool share_cacheline, bool accept_UB> \
  requires(std::has_single_bit(length_)) && \
  std::is_default_constructible_v<ContentType_> &&  \
  std::is_trivially_copyable_v<ContentType_> && \
  (!std::is_const_v<ContentType_>)&& \
  std::atomic<std::int64_t>::is_always_lock_free

#define SEQ_LOCK_QUEUE                                                         \
  Queue::SeqLockQueue<ContentType_, length_, share_cacheline, accept_UB>

TEMPLATE_PARAMS
SEQ_LOCK_QUEUE::SeqLockQueue()
    :memory_pointer{new(std::align_val_t{cacheline}) ElementType[length]},
     dequeue_span{this->memory_pointer, length},
     enqueue_span{this->memory_pointer, length} {};

TEMPLATE_PARAMS
SEQ_LOCK_QUEUE::~SeqLockQueue() { delete[] this->memory_pointer; };

TEMPLATE_PARAMS
SEQ_LOCK_QUEUE::ReadReturnType SEQ_LOCK_QUEUE::read_element(std::int64_t read_index, std::int64_t prev_version) const noexcept {
  return this->dequeue_span[read_index % length].read(prev_version);
};

TEMPLATE_PARAMS
void SEQ_LOCK_QUEUE::enqueue(const ContentType_ content_) noexcept {
  this->enqueue_span[this->enqueue_index % length].insert(content_);
  ++this->enqueue_index;
};

TEMPLATE_PARAMS
SEQ_LOCK_QUEUE::QueueReader::QueueReader(const SEQ_LOCK_QUEUE* queue_ptr_) noexcept
: queue_ptr(queue_ptr_){};

TEMPLATE_PARAMS
std::optional<ContentType_> SEQ_LOCK_QUEUE::QueueReader::read_next_entry() noexcept{
  const auto read_result = this->queue_ptr->read_element(this->read_index, this->prev_version);
  const auto ret_opt = std::get<0>(read_result);
  const bool new_entry_read = ret_opt.has_value();
  this->prev_version = new_entry_read ?
      std::get<1>(read_result)
      // when buffer position wraps, the version number of the next read must be larger than the previous one or the first entry in the buffer will be read twice
      + 2 * (this->read_index % length == length - 1)
      : this->prev_version;
  this->read_index += new_entry_read;
  return ret_opt;
};

TEMPLATE_PARAMS
SEQ_LOCK_QUEUE::QueueReader SEQ_LOCK_QUEUE::get_reader() const noexcept{
  return QueueReader(this);
};


#undef TEMPLATE_PARAMS
#undef SEQ_LOCK_QUEUE
