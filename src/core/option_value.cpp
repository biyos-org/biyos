#include "core/option_value.hpp"

#include "core/error.hpp"
#include "core/option_table.hpp"
#include "core/option_text.hpp"
#include "core/text.hpp"
#include "core/version.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace biyos
{

namespace
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

void check_value(const OptionSpec &spec, const EnumRule &rule, std::string_view value)
{
    if (std::ranges::find(rule.values, value) != rule.values.end())
    {
        return;
    }
    throw Error(ErrorKind::InvalidArgument,
                subject(spec.name),
                "'" + std::string(value) + "' is not one of " + joined(rule.values));
}

const OptionSpec &settable(std::span<const OptionSpec> table, Option option)
{
    const OptionSpec &spec = row_of(table, option);
    if (!spec.versions.retired)
    {
        return spec;
    }
    const std::string_view replacement = row_of(table, *spec.replacement).name;
    const std::string reason = notice("retired", *spec.versions.retired, replacement);
    throw Error(ErrorKind::InvalidArgument, subject(spec.name), reason);
}

void admit(const OptionSpec &spec, bool)
{
    rule_of<BoolRule>(spec);
}

void admit(const OptionSpec &spec, int value)
{
    check_range(spec, rule_of<IntRule>(spec), value);
}

void admit(const OptionSpec &spec, double value)
{
    check_range(spec, rule_of<DoubleRule>(spec), value);
}

void admit(const OptionSpec &spec, const std::string &value)
{
    if (const EnumRule *rule = std::get_if<EnumRule>(&spec.rule))
    {
        check_value(spec, *rule, value);
    }
    else
    {
        rule_of<StringRule>(spec);
    }
}

void describe(Event &event, std::span<const OptionSpec> table, const OptionSpec &spec)
{
    const Version deprecated = *spec.versions.deprecated;
    const std::string_view replacement = row_of(table, *spec.replacement).name;
    event.module = "core";
    event.text = subject(spec.name) + ": " + notice("deprecated", deprecated, replacement);
    event.fields.push_back(Field{.name = "option", .value = std::string(spec.name)});
    event.fields.push_back(Field{.name = "deprecated", .value = dotted(deprecated)});
    event.fields.push_back(Field{.name = "replacement", .value = std::string(replacement)});
}

}

const OptionSpec &checked_row(std::span<const OptionSpec> table,
                              Option option,
                              const OptionValue &value)
{
    const OptionSpec &spec = settable(table, option);
    std::visit([&spec](const auto &held) { admit(spec, held); }, value);
    return spec;
}

void warn_deprecated(Events &events,
                     std::span<const OptionSpec> table,
                     const OptionSpec &spec) noexcept
{
    if (!spec.versions.deprecated)
    {
        return;
    }
    const auto fill = [&](Event &event) { describe(event, table, spec); };
    events.emit(EventKind::Message, EventLevel::Warning, fill);
}

bool bool_of(const OptionSpec &spec, const OptionValue &value)
{
    rule_of<BoolRule>(spec);
    return std::get<bool>(value);
}

int int_of(const OptionSpec &spec, const OptionValue &value)
{
    rule_of<IntRule>(spec);
    return std::get<int>(value);
}

double double_of(const OptionSpec &spec, const OptionValue &value)
{
    rule_of<DoubleRule>(spec);
    return std::get<double>(value);
}

const std::string &string_of(const OptionSpec &spec, const OptionValue &value)
{
    if (!std::holds_alternative<EnumRule>(spec.rule))
    {
        rule_of<StringRule>(spec);
    }
    return std::get<std::string>(value);
}

std::size_t position_of(const OptionSpec &spec, const OptionValue &value)
{
    const EnumRule &rule = rule_of<EnumRule>(spec);
    const auto found = std::ranges::find(rule.values, std::get<std::string>(value));
    return static_cast<std::size_t>(found - rule.values.begin());
}

}
