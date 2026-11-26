#include "core/events/deliveries.hpp"

#include "core/thread.hpp"

#include <mutex>

namespace biyos
{

Deliveries::Entry::Entry(Deliveries &deliveries) noexcept
    : deliveries_(deliveries), thread_(this_thread()), state_(&own_)
{
    const std::lock_guard<std::mutex> lock(deliveries_.lock_);
    const Entry *const outer = deliveries_.entry_of(thread_);
    if (outer != nullptr)
    {
        state_ = outer->state_;
        return;
    }
    next_ = deliveries_.first_;
    deliveries_.first_ = this;
    ++deliveries_.count_;
}

Deliveries::Entry::~Entry()
{
    if (state_ != &own_)
    {
        return;
    }
    const std::lock_guard<std::mutex> lock(deliveries_.lock_);
    Entry **link = &deliveries_.first_;
    while (*link != this)
    {
        link = &(*link)->next_;
    }
    *link = next_;
    --deliveries_.count_;
}

bool Deliveries::delivering() const noexcept
{
    const State *const state = find();
    return state != nullptr && state->depth > 0;
}

bool Deliveries::filling() const noexcept
{
    const State *const state = find();
    return state != nullptr && state->fills > 0;
}

Deliveries::State *Deliveries::delivery() noexcept
{
    State *const state = find();
    return state != nullptr && state->depth > 0 ? state : nullptr;
}

Deliveries::State *Deliveries::find() const noexcept
{
    if (count_.load() == 0)
    {
        return nullptr;
    }
    const std::lock_guard<std::mutex> lock(lock_);
    const Entry *const entry = entry_of(this_thread());
    return entry == nullptr ? nullptr : entry->state_;
}

Deliveries::Entry *Deliveries::entry_of(ThreadId thread) const noexcept
{
    Entry *entry = first_;
    while (entry != nullptr && entry->thread_ != thread)
    {
        entry = entry->next_;
    }
    return entry;
}

}
