#pragma once

#include "core/events.hpp"
#include "core/options.hpp"

#include <span>

namespace biyos
{

void warn_deprecated(Events &events, std::span<const OptionSpec> table,
                     const OptionSpec &spec) noexcept;

}
