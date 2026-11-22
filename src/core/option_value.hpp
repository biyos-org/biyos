#pragma once

#include "core/error.hpp"
#include "core/events.hpp"
#include "core/option_text.hpp"
#include "core/options.hpp"
#include "core/text.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace biyos
{

template <typename Rule> const Rule &rule_of(const OptionSpec &spec)
{
    if (const Rule *rule = std::get_if<Rule>(&spec.rule))
    {
        return *rule;
    }
    const std::string held(noun(spec));
    const std::string wanted(noun(Rule{}));
    throw Error(ErrorKind::InvalidArgument, subject(spec.name), "is " + held + ", not " + wanted);
}

template <typename Rule, typename T>
void check_range(const OptionSpec &spec, const Rule &rule, T value)
{
    if (rule.lower <= value && value <= rule.upper)
    {
        return;
    }
    const std::string range = "[" + text_of(rule.lower) + ", " + text_of(rule.upper) + "]";
    throw Error(ErrorKind::InvalidArgument,
                subject(spec.name),
                text_of(value) + " is outside " + range);
}

void check_value(const OptionSpec &spec, const EnumRule &rule, std::string_view value);
const OptionSpec &settable(std::span<const OptionSpec> table, Option option);

void warn_deprecated(Events &events,
                     std::span<const OptionSpec> table,
                     const OptionSpec &spec) noexcept;

bool bool_of(const OptionSpec &spec, const OptionValue &value);
int int_of(const OptionSpec &spec, const OptionValue &value);
double double_of(const OptionSpec &spec, const OptionValue &value);
const std::string &string_of(const OptionSpec &spec, const OptionValue &value);
std::size_t position_of(const OptionSpec &spec, const OptionValue &value);

}
