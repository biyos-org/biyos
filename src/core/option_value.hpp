#pragma once

#include "core/events.hpp"
#include "core/options.hpp"

#include <span>
#include <string>

namespace biyos
{

const OptionSpec &checked_row(std::span<const OptionSpec> table,
                              Option option,
                              const OptionValue &value);

void warn_deprecated(Events &events,
                     std::span<const OptionSpec> table,
                     const OptionSpec &spec) noexcept;

bool bool_of(const OptionSpec &spec, const OptionValue &value);
int int_of(const OptionSpec &spec, const OptionValue &value);
double double_of(const OptionSpec &spec, const OptionValue &value);
const std::string &string_of(const OptionSpec &spec, const OptionValue &value);
int enum_of(const OptionSpec &spec, const OptionValue &value);
Masks flags_of(const OptionSpec &spec, const OptionValue &value);
PartState state_of(const OptionSpec &spec, const OptionValue &value, Mask part);

}
