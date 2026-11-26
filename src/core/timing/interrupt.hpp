#pragma once

#include <atomic>

namespace biyos
{

class Interrupt
{
  public:
    void request() noexcept { count_.fetch_add(1, std::memory_order_relaxed); }
    unsigned count() const noexcept { return count_.load(std::memory_order_relaxed); }

  private:
    static_assert(std::atomic<unsigned>::is_always_lock_free);

    std::atomic<unsigned> count_ = 0;
};

}
