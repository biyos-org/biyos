#pragma once

#include "core/env/thread.hpp"

#include <atomic>

namespace biyos
{

class Usage
{
  public:
    bool enter() noexcept;
    bool claim() noexcept;
    void leave() noexcept;
    bool held() const noexcept;

  private:
    std::atomic<int> calls_{0};
    std::atomic<ThreadId> owner_{};
    int held_ = 0;
};

}
