#pragma once

#include "core/events/deliveries.hpp"
#include "core/events/event.hpp"
#include "core/events/hooks.hpp"
#include "core/timing/clock.hpp"

#include <chrono>

namespace biyos
{

class Events
{
  public:
    explicit Events(const Clock &clock);

    int add_hook(EventKind kind, EventLevel level, Hook hook);
    void delete_hook(int id);

    bool wanted(EventKind kind, EventLevel level) const noexcept
    {
        return hooks_.wanted(kind, level);
    }
    bool delivering() const noexcept { return deliveries_.delivering(); }
    bool filling() const noexcept { return deliveries_.filling(); }
    Deliveries::State *delivery() noexcept { return deliveries_.delivery(); }
    std::chrono::nanoseconds origin() const noexcept { return origin_; }

  private:
    friend class Stream;

    std::chrono::nanoseconds origin_;
    Hooks hooks_;
    Deliveries deliveries_;
};

}
