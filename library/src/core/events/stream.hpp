#pragma once

#include "core/env.hpp"
#include "core/error.hpp"
#include "core/events/deliveries.hpp"
#include "core/events/event.hpp"
#include "core/events/events.hpp"
#include "core/events/hooks.hpp"
#include "core/memory/depth.hpp"
#include "core/timing/clock.hpp"

#include <chrono>
#include <concepts>
#include <string_view>

namespace biyos
{

class Session;

class Stream
{
  public:
    bool wanted(EventKind kind, EventLevel level) const noexcept;

    template <std::invocable<Event &> Fill>
    void emit(EventKind kind, EventLevel level, Fill &&fill) const noexcept
    {
        if (!wanted(kind, level))
        {
            return;
        }
        const Deliveries::Entry entry(env_.events().deliveries_);
        Event event = blank(kind, level);
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
        deliver(event);
    }

    void record(ErrorKind kind, std::string_view text) const noexcept;
    const Clock &clock() const noexcept;
    std::chrono::nanoseconds origin() const noexcept;
    bool delivering() const noexcept;
    bool filling() const noexcept;
    const Session *session() const noexcept;

  private:
    friend class Env;
    friend class Session;

    explicit Stream(Env &env) noexcept;
    Stream(Env &env, Hooks &hooks, const Session &session) noexcept;

    Event blank(EventKind kind, EventLevel level) const noexcept;
    void deliver(const Event &event) const noexcept;

    Env &env_;
    Hooks *hooks_;
    const Session *session_;
};

}
