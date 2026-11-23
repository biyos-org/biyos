#pragma once

#include "core/clock.hpp"
#include "core/deliveries.hpp"
#include "core/depth.hpp"
#include "core/event.hpp"
#include "core/hooks.hpp"
#include "core/thread.hpp"

#include <atomic>
#include <chrono>
#include <concepts>
#include <mutex>

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
    void emit(EventKind kind, EventLevel level, Fill &&fill) noexcept
    {
        if (!wanted(kind, level))
        {
            return;
        }
        const Deliveries::Entry entry(deliveries_);
        const Turn turn(*this);
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
        hooks_.deliver(event);
    }

  private:
    class Turn
    {
      public:
        explicit Turn(Events &events) noexcept;
        ~Turn();
        Turn(const Turn &) = delete;
        Turn &operator=(const Turn &) = delete;

      private:
        Events &events_;
        bool nested_;
    };

    bool mine() const noexcept { return owner_.load() == this_thread(); }

    const Clock &clock_;
    std::chrono::nanoseconds origin_;
    Hooks hooks_;
    Deliveries deliveries_;
    std::mutex turn_;
    std::atomic<ThreadId> owner_;
};

}
