#pragma once

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/options/options.hpp"
#include "core/text/strings.hpp"

#include <span>
#include <string_view>

namespace biyos
{

const OptionSpec &checked_row(std::span<const OptionSpec> table,
                              Option option,
                              const OptionValue &value);

void warn_deprecated(Stream stream,
                     std::span<const OptionSpec> table,
                     const OptionSpec &spec) noexcept;

OptionValue kept(Strings &strings, const OptionValue &value);

bool bool_of(const OptionSpec &spec, const OptionValue &value);
int int_of(const OptionSpec &spec, const OptionValue &value);
Real double_of(const OptionSpec &spec, const OptionValue &value);
std::string_view string_of(const OptionSpec &spec, const OptionValue &value);
int enum_of(const OptionSpec &spec, const OptionValue &value);
Masks flags_of(const OptionSpec &spec, const OptionValue &value);
PartState state_of(const OptionSpec &spec, const OptionValue &value, Mask part);

}
