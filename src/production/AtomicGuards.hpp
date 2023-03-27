#pragma once

#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>

#include "Auxil.hpp"

namespace AtomicGuards {
struct AtomicFlagGuard {
 private:
  std::atomic_flag* flag_ptr;
  bool flag_locked;

 public:
  bool is_locked() noexcept;
  bool lock() noexcept;
  bool unlock() noexcept;
  std::atomic_flag* return_flag_ptr() noexcept;
  bool is_valid() const noexcept;
  void rebind(std::atomic_flag*) noexcept;
  explicit AtomicFlagGuard(std::atomic_flag* flag_ptr);
  ~AtomicFlagGuard() noexcept;
  AtomicFlagGuard(const AtomicFlagGuard&) = delete;
  AtomicFlagGuard& operator=(const AtomicFlagGuard&) = delete;
  AtomicFlagGuard(AtomicFlagGuard&&);
  AtomicFlagGuard& operator=(AtomicFlagGuard&&);
};
}  // namespace AtomicGuards

namespace AtomicGuards {
bool AtomicFlagGuard::is_locked() noexcept { return this->flag_locked; }

bool AtomicFlagGuard::lock() noexcept {
  bool false_{false};
  if (this->flag_ptr != nullptr) {
    while (std::atomic_flag_test_and_set_explicit(this->flag_ptr,
                                                  std::memory_order_acquire))
      ;
    this->flag_locked = true;
    return true;
  } else {
    return false;
  }
}

bool AtomicFlagGuard::unlock() noexcept {
  if ((this->flag_locked) && (this->flag_ptr != nullptr)) {
    bool true_{true};
    this->flag_locked = false;
    std::atomic_flag_clear_explicit(this->flag_ptr, std::memory_order_release);
    return true;
  }
  return false;
}

std::atomic_flag* AtomicFlagGuard::return_flag_ptr() noexcept {
  return this->flag_ptr;
}

bool AtomicFlagGuard::is_valid() const noexcept {
  return this->flag_ptr != nullptr;
}

void AtomicFlagGuard::rebind(std::atomic_flag* otherflag_ptr) noexcept {
  this->unlock();
  this->flag_ptr = otherflag_ptr;
}

AtomicFlagGuard::AtomicFlagGuard(std::atomic_flag* flag_ptr)
    : flag_ptr(flag_ptr), flag_locked(false){};

AtomicFlagGuard::~AtomicFlagGuard() noexcept { this->unlock(); }

AtomicFlagGuard::AtomicFlagGuard(AtomicFlagGuard&& other)
    : flag_ptr{other.flag_ptr}, flag_locked{other.flag_locked} {
  other.flag_ptr = nullptr;
  other.flag_locked = false;
}

AtomicFlagGuard& AtomicFlagGuard::operator=(AtomicFlagGuard&& other) {
  this->unlock();
  this->rebind(other.return_flag_ptr());
  this->flag_locked = other.flag_locked;
  other.flag_ptr = nullptr;
  other.flag_locked = false;
  return *this;
}
}  // namespace AtomicGuards