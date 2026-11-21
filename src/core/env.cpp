#include "core/env.hpp"

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

Env::Env() : Env(std::make_unique<SystemClock>()) {}

Env::Env(std::unique_ptr<Clock> clock)
    : clock_(checked(std::move(clock))), events_(*clock_), options_(events_)
{
}

Env::Env(std::unique_ptr<Clock> clock, std::span<const OptionSpec> table)
    : clock_(checked(std::move(clock))), events_(*clock_), options_(table, events_)
{
}

Options &Env::options() noexcept
{
    return options_;
}

const Options &Env::options() const noexcept
{
    return options_;
}

const Clock &Env::clock() const noexcept
{
    return *clock_;
}

Events &Env::events() noexcept
{
    return events_;
}

const Events &Env::events() const noexcept
{
    return events_;
}

const std::string &Env::last_error() const noexcept
{
    return last_error_;
}

Env::ErrorScope::ErrorScope(Env &env) noexcept : env_(env), outer_(env.scope_)
{
    env_.scope_ = this;
}

Env::ErrorScope::~ErrorScope()
{
    if (swapped_)
    {
        env_.last_error_.swap(kept_);
    }
    env_.scope_ = outer_;
}

void Env::record_error(std::string_view text) noexcept
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

std::size_t Env::handles() const noexcept
{
    return handles_;
}

Env::Handle::Handle(Env &env) noexcept : env_(env)
{
    ++env_.handles_;
}

Env::Handle::~Handle()
{
    --env_.handles_;
}

Env &Env::Handle::env() const noexcept
{
    return env_;
}

}
