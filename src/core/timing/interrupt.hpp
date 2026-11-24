#pragma once

#include <atomic>

namespace biyos
{

class Interrupt
{
  public:
    void set() noexcept { flag_.store(true, std::memory_order_relaxed); }
    void clear() noexcept { flag_.store(false, std::memory_order_relaxed); }
    bool is_set() const noexcept { return flag_.load(std::memory_order_relaxed); }

  private:
    static_assert(std::atomic<bool>::is_always_lock_free);

    std::atomic<bool> flag_ = false;
};

}
