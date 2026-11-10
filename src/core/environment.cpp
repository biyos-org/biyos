#include "core/environment.hpp"

namespace biyos
{

Options &Environment::options() noexcept
{
    return options_;
}

const Options &Environment::options() const noexcept
{
    return options_;
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

}
