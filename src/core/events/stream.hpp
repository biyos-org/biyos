#pragma once

#include "core/env.hpp"
#include "core/error.hpp"
#include "core/events/event.hpp"
#include "core/events/hooks.hpp"
#include "core/timing/clock.hpp"

#include <chrono>
#include <concepts>
#include <string_view>
#include <utility>

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
        const auto named = [this, &fill](Event &event) { fill(name(event)); };
        env_.events().emit(kind, level, named, hooks_);
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

    Event &name(Event &event) const noexcept;

    Env &env_;
    Hooks *hooks_;
    const Session *session_;
};

}
