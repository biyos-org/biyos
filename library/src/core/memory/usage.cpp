#include "core/memory/usage.hpp"

#include "core/env/thread.hpp"

namespace biyos
{

bool Usage::enter() noexcept
{
    ++calls_;
    const ThreadId owner = owner_.load();
    if (owner == ThreadId{})
    {
        return true;
    }
    if (owner == this_thread())
    {
        ++held_;
        return true;
    }
    --calls_;
    return false;
}

bool Usage::claim() noexcept
{
    const ThreadId self = this_thread();
    if (owner_.load() == self)
    {
        return true;
    }
    ThreadId free{};
    if (!owner_.compare_exchange_strong(free, self))
    {
        return false;
    }
    if (calls_.load() != 1)
    {
        owner_.store(ThreadId{});
        return false;
    }
    held_ = 1;
    return true;
}

void Usage::leave() noexcept
{
    if (owner_.load() == this_thread() && --held_ == 0)
    {
        owner_.store(ThreadId{});
    }
    --calls_;
}

bool Usage::held() const noexcept
{
    return owner_.load() == this_thread();
}

}
