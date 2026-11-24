#pragma once

#include "core/events/deliveries.hpp"
#include "core/events/event.hpp"
#include "core/events/hooks.hpp"
#include "core/memory/depth.hpp"
#include "core/timing/clock.hpp"

#include <chrono>
#include <concepts>

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

    template <std::invocable<Event &> Fill>
    void emit(EventKind kind, EventLevel level, Fill &&fill, Hooks *model_hooks = nullptr) noexcept
    {
        const bool local = model_hooks != nullptr && model_hooks->wanted(kind, level);
        if (!local && !wanted(kind, level))
        {
            return;
        }
        const Deliveries::Entry entry(deliveries_);
        Event event{
            .kind = kind,
            .level = level,
            .time = clock_.wall_time() - origin_,
            .module = {},
            .text = {},
            .fields = {},
            .model = nullptr,
        };
        try
        {
            const Depth inside(entry.state().fills);
            fill(event);
        }
        catch (...)
        {
            return;
        }
        const Depth delivery(entry.state().depth);
        if (model_hooks != nullptr)
        {
            model_hooks->deliver(event);
        }
        hooks_.deliver(event);
    }

  private:
    const Clock &clock_;
    std::chrono::nanoseconds origin_;
    Hooks hooks_;
    Deliveries deliveries_;
};

}
