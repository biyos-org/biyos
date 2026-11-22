#include "core/option_value.hpp"

#include "core/error.hpp"
#include "core/option_table.hpp"
#include "core/option_text.hpp"
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
