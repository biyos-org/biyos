#include "core/events/stream.hpp"

#include <chrono>
#include <string_view>

namespace biyos
{

Stream::Stream(Env &env) noexcept : env_(env), hooks_(nullptr), session_(nullptr) {}

Stream::Stream(Env &env, Hooks &hooks, const Session &session) noexcept
    : env_(env), hooks_(&hooks), session_(&session)
{
}

bool Stream::wanted(EventKind kind, EventLevel level) const noexcept
{
    const bool local = hooks_ != nullptr && hooks_->wanted(kind, level);
    return local || env_.events().wanted(kind, level);
}

void Stream::record(ErrorKind kind, std::string_view text) const noexcept
{
    env_.record_error(kind, text, session_);
}

const Clock &Stream::clock() const noexcept
{
    return env_.clock();
}

std::chrono::nanoseconds Stream::origin() const noexcept
{
    return env_.events().origin();
}

bool Stream::delivering() const noexcept
{
    return env_.events().delivering();
}

bool Stream::filling() const noexcept
{
    return env_.events().filling();
}

Event &Stream::name(Event &event) const noexcept
{
    event.model = session_;
    return event;
}

const Session *Stream::session() const noexcept
{
    return session_;
}

}
