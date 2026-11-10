#include "core/environment.hpp"

#include "core/error.hpp"

#include <cstddef>
#include <memory>
#include <utility>

namespace biyos
{

namespace
{

std::unique_ptr<Clock> checked(std::unique_ptr<Clock> clock)
{
    if (!clock)
    {
        throw Error(ErrorKind::InvalidArgument, "clock", "is null");
    }
    return clock;
}

}

Environment::Environment() : Environment(std::make_unique<SystemClock>()) {}

Environment::Environment(std::unique_ptr<Clock> clock)
    : clock_(checked(std::move(clock))), events_(*clock_), options_(events_)
{
}

Environment::Environment(std::unique_ptr<Clock> clock, std::span<const OptionSpec> table)
    : clock_(checked(std::move(clock))), events_(*clock_), options_(table, events_)
{
}

Options &Environment::options() noexcept
{
    return options_;
}

const Options &Environment::options() const noexcept
{
    return options_;
}

const Clock &Environment::clock() const noexcept
{
    return *clock_;
}

Events &Environment::events() noexcept
{
    return events_;
}

const Events &Environment::events() const noexcept
{
    return events_;
}

const std::string &Environment::last_error() const noexcept
{
    return last_error_;
}

Environment::ErrorScope::ErrorScope(Environment &env) noexcept : env_(env), outer_(env.scope_)
{
    env_.scope_ = this;
}

Environment::ErrorScope::~ErrorScope()
{
    if (swapped_)
    {
        env_.last_error_.swap(kept_);
    }
    env_.scope_ = outer_;
}

void Environment::record_error(std::string_view text) noexcept
{
    if (scope_ != nullptr && !scope_->swapped_)
    {
        last_error_.swap(scope_->kept_);
        scope_->swapped_ = true;
    }
    try
    {
        last_error_.assign(text);
    }
    catch (...)
    {
        last_error_.clear();
    }
}

std::size_t Environment::handles() const noexcept
{
    return handles_;
}

Environment::Handle::Handle(Environment &env) noexcept : env_(env)
{
    ++env_.handles_;
}

Environment::Handle::~Handle()
{
    --env_.handles_;
}

Environment &Environment::Handle::env() const noexcept
{
    return env_;
}

}
