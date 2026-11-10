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
