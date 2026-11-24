#include "core/options/value.hpp"

#include "core/error.hpp"
#include "core/options/table.hpp"
#include "core/options/text.hpp"
#include "core/text.hpp"
#include "core/version.hpp"

#include <algorithm>
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

void check_value(const OptionSpec &spec, const EnumRule &rule, int value)
{
    if (std::ranges::find(rule.values, value, &EnumValue::number) != rule.values.end())
    {
        return;
    }
    throw Error(ErrorKind::InvalidArgument,
                subject(spec.name),
                text_of(value) + " is not one of " + joined(rule.values));
}

void check_value(const OptionSpec &spec, const FlagsRule &rule, const Masks &value)
{
    Mask listed = 0;
    for (const FlagPart &part : rule.parts)
    {
        listed |= part.bit;
    }
    if (const Mask outside = (value.on | value.off) & ~listed; outside != 0)
    {
        const std::string reason = "sets " + bits(outside) + " outside its parts ";
        throw Error(ErrorKind::InvalidArgument, subject(spec.name), reason + joined(rule.parts));
    }
    if (const Mask both = value.on & value.off; both != 0)
    {
        const std::string reason = "sets " + bits(both) + " both on and off";
        throw Error(ErrorKind::InvalidArgument, subject(spec.name), reason);
    }
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
    if (const EnumRule *rule = std::get_if<EnumRule>(&spec.rule))
    {
        check_value(spec, *rule, value);
    }
    else
    {
        check_range(spec, rule_of<IntRule>(spec), value);
    }
}

void admit(const OptionSpec &spec, double value)
{
    check_range(spec, rule_of<DoubleRule>(spec), value);
}

void admit(const OptionSpec &spec, std::string_view)
{
    rule_of<StringRule>(spec);
}

void admit(const OptionSpec &spec, const Masks &value)
{
    check_value(spec, rule_of<FlagsRule>(spec), value);
}

void describe(Event &event,
              std::span<const OptionSpec> table,
              const OptionSpec &spec,
              const BIYOS_model *model)
{
    const Version deprecated = *spec.versions.deprecated;
    const std::string_view replacement = row_of(table, *spec.replacement).name;
    event.module = "core";
    event.text = subject(spec.name) + ": " + notice("deprecated", deprecated, replacement);
    event.fields.push_back(Field{.name = "option", .value = std::string(spec.name)});
    event.fields.push_back(Field{.name = "deprecated", .value = dotted(deprecated)});
    event.fields.push_back(Field{.name = "replacement", .value = std::string(replacement)});
    event.model = model;
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
                     const OptionSpec &spec,
                     const BIYOS_model *model,
                     Hooks *hooks) noexcept
{
    if (!spec.versions.deprecated)
    {
        return;
    }
    const auto fill = [&](Event &event) { describe(event, table, spec, model); };
    events.emit(EventKind::Message, EventLevel::Warning, fill, hooks);
}

OptionValue kept(Strings &strings, const OptionValue &value)
{
    if (const std::string_view *text = std::get_if<std::string_view>(&value))
    {
        return strings.keep(*text);
    }
    return value;
}

bool bool_of(const OptionSpec &spec, const OptionValue &value)
{
    rule_of<BoolRule>(spec);
    return std::get<bool>(value);
}

int int_of(const OptionSpec &spec, const OptionValue &value)
{
    if (!std::holds_alternative<EnumRule>(spec.rule))
    {
        rule_of<IntRule>(spec);
    }
    return std::get<int>(value);
}

double double_of(const OptionSpec &spec, const OptionValue &value)
{
    rule_of<DoubleRule>(spec);
    return std::get<double>(value);
}

std::string_view string_of(const OptionSpec &spec, const OptionValue &value)
{
    rule_of<StringRule>(spec);
    return std::get<std::string_view>(value);
}

int enum_of(const OptionSpec &spec, const OptionValue &value)
{
    rule_of<EnumRule>(spec);
    return std::get<int>(value);
}

Masks flags_of(const OptionSpec &spec, const OptionValue &value)
{
    rule_of<FlagsRule>(spec);
    return std::get<Masks>(value);
}

PartState state_of(const OptionSpec &spec, const OptionValue &value, Mask part)
{
    const FlagsRule &rule = rule_of<FlagsRule>(spec);
    if (std::ranges::find(rule.parts, part, &FlagPart::bit) == rule.parts.end())
    {
        throw Error(ErrorKind::Internal, subject(spec.name), "has no part " + text_of(part));
    }
    const Masks &masks = std::get<Masks>(value);
    if ((masks.on & part) != 0)
    {
        return PartState::On;
    }
    if ((masks.off & part) != 0)
    {
        return PartState::Off;
    }
    return PartState::Default;
}

}
