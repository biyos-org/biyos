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

void Environment::record_error(std::string_view text) noexcept
{
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
