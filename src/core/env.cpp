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

Interrupt &Env::interrupt() noexcept
{
    return interrupt_;
}

const Interrupt &Env::interrupt() const noexcept
{
    return interrupt_;
}

RecordView Env::last_error() const noexcept
{
    return records_.get();
}

ErrorScope::ErrorScope(Env &env) noexcept
    : env_(env), delivery_(env.events().delivery()),
      outer_(delivery_ == nullptr ? nullptr : delivery_->scope)
{
    if (delivery_ != nullptr)
    {
        delivery_->scope = this;
    }
}

ErrorScope::~ErrorScope()
{
    if (swapped_)
    {
        env_.records_.swap(kept_);
    }
    if (delivery_ != nullptr)
    {
        delivery_->scope = outer_;
    }
}

ErrorScope *Env::scope() noexcept
{
    const Deliveries::State *const delivery = events_.delivery();
    return delivery == nullptr ? nullptr : delivery->scope;
}

void Env::record_error(ErrorKind kind, std::string_view text, const BIYOS_model *model) noexcept
{
    ErrorScope *const open = scope();
    if (open != nullptr && !open->swapped_)
    {
        open->swapped_ = true;
        records_.swap(open->kept_);
    }
    records_.put(status_of(kind), model, text);
}

std::size_t Env::handles() const noexcept
{
    return handles_.load();
}

Usage &Env::usage() noexcept
{
    return usage_;
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
