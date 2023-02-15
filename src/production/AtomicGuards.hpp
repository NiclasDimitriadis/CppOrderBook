#pragma once

#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>

#include "Auxil.hpp"

namespace AtomicGuards{
    struct AtomicFlagGuard
    {
    private:
		static_assert(std::endian::native == std::endian::little);
        std::atomic<bool>* flag_ptr;
        bool flag_locked;

    public:
        bool is_locked() noexcept;
        bool lock() noexcept;
        bool unlock() noexcept;
        std::atomic<bool>* return_flag_ptr() noexcept;
		bool is_valid() const noexcept;
        void rebind(std::atomic_flag*) noexcept;
        explicit AtomicFlagGuard(std::atomic_flag* flag_ptr);
        ~AtomicFlagGuard() noexcept;
        AtomicFlagGuard(const AtomicFlagGuard&) = delete;
        AtomicFlagGuard& operator=(const AtomicFlagGuard&) = delete;
        AtomicFlagGuard(AtomicFlagGuard&&);
        AtomicFlagGuard& operator=(AtomicFlagGuard&&);
    };