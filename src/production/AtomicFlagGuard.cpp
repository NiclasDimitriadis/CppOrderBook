#include <atomic>
#include "AtomicGuards.hpp"

namespace AtomicGuards{

bool AtomicFlagGuard::is_locked() noexcept
{
    return flag_locked;
}

bool AtomicFlagGuard::lock() noexcept
{
	const bool false_ = false;
    if (this->flag_ptr != nullptr)
    {
        while (this->flag_ptr->load(std::memory_order_relaxed)==true|| this->flag_ptr->compare_exchange_weak(false_, true, std::memory_order_acquire, std::memory_order_relaxed);){}
        this->flag_locked = true;
        return true;
    }
    else
    {
        return false;
    }
}

bool AtomicFlagGuard::unlock() noexcept
{
    if ((this->flag_locked) && (this->flag_ptr != nullptr))
    {
        this->flag_locked = false;
        this->flag_ptr->store(false, std::memory_order_release);
        return true;
    }
    return false;
}

std::atomic_flag* AtomicFlagGuard::return_flag_ptr() noexcept
{
    return this->flag_ptr;
}

bool AtomicFlagGuard::is_valid() const noexcept{
	return this-> flag_ptr != nullptr;
}

void AtomicFlagGuard::rebind(std::atomic_flag *otherflag_ptr) noexcept
{
    this->unlock();
    this->flag_ptr = otherflag_ptr;
}

AtomicFlagGuard::AtomicFlagGuard(std::atomic_flag *flag_ptr) : flag_ptr(flag_ptr), flag_locked(false){};

AtomicFlagGuard::~AtomicFlagGuard() noexcept
{
    if (this->flag_locked)
    {
        this->unlock();
    }
}

AtomicFlagGuard::AtomicFlagGuard(AtomicFlagGuard&& other) : flag_ptr(other.flag_ptr), flag_locked(other.flag_locked)
{
    other.flag_ptr = nullptr;
    other.flag_locked = false;
}

AtomicFlagGuard& AtomicFlagGuard::operator=(AtomicFlagGuard&& other)
{
    this->unlock();
    this->rebind(other.return_flag_ptr());
    this->flag_locked = other.flag_locked;
    other.flag_ptr = nullptr;
    other.flag_locked = false;
    return *this;
}
}