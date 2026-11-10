#include "core/options.hpp"

#include "core/error.hpp"
#include "core/option_checks.hpp"
#include "core/option_table.hpp"
#include "core/option_text.hpp"
#include "core/version.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace biyos
{

namespace
{

bool default_of(const BoolRule &rule)
{
    return rule.default_value;
}

int default_of(const IntRule &rule)
{
    return rule.default_value;
}

double default_of(const DoubleRule &rule)
{
    return rule.default_value;
}

std::string default_of(const StringRule &rule)
{
    return std::string(rule.default_value);
}

std::string default_of(const EnumRule &rule)
{
    return std::string(rule.default_value);
}

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
    throw Error(ErrorKind::InvalidArgument, subject(spec.name),
                text_of(value) + " is outside " + range);
}

void check_value(const OptionSpec &spec, const EnumRule &rule, std::string_view value)
{
    if (std::ranges::find(rule.values, value) != rule.values.end())
    {
        return;
    }
    throw Error(ErrorKind::InvalidArgument, subject(spec.name),
                "'" + std::string(value) + "' is not one of " + joined(rule.values));
}

std::span<const OptionSpec> checked(std::span<const OptionSpec> table)
{
    if (!well_formed(table, version()))
    {
        throw Error(ErrorKind::Internal, "option table", "is not well formed");
    }
    return table;
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

}

Options::Options() : table_(option_table()), values_(defaults(table_)) {}

Options::Options(std::span<const OptionSpec> table)
    : table_(checked(table)), values_(defaults(table_))
{
}

std::vector<Options::Value> Options::defaults(std::span<const OptionSpec> table)
{
    std::vector<Value> values;
    values.reserve(table.size());
    const auto value_of = [](const auto &rule) { return Value(default_of(rule)); };
    for (const OptionSpec &spec : table)
    {
        values.push_back(std::visit(value_of, spec.rule));
    }
    return values;
}

void Options::set_bool(Option option, bool value)
{
    const OptionSpec &spec = settable(table_, option);
    rule_of<BoolRule>(spec);
    store(spec, value);
}

void Options::set_int(Option option, int value)
{
    const OptionSpec &spec = settable(table_, option);
    check_range(spec, rule_of<IntRule>(spec), value);
    store(spec, value);
}

void Options::set_double(Option option, double value)
{
    const OptionSpec &spec = settable(table_, option);
    check_range(spec, rule_of<DoubleRule>(spec), value);
    store(spec, value);
}

void Options::set_string(Option option, std::string_view value)
{
    const OptionSpec &spec = settable(table_, option);
    if (const EnumRule *rule = std::get_if<EnumRule>(&spec.rule))
    {
        check_value(spec, *rule, value);
    }
    else
    {
        rule_of<StringRule>(spec);
    }
    store(spec, std::string(value));
}

void Options::store(const OptionSpec &spec, Value value)
{
    values_[index_of(spec.id)] = std::move(value);
}

bool Options::get_bool(Option option) const
{
    rule_of<BoolRule>(row_of(table_, option));
    return std::get<bool>(values_[index_of(option)]);
}

int Options::get_int(Option option) const
{
    rule_of<IntRule>(row_of(table_, option));
    return std::get<int>(values_[index_of(option)]);
}

double Options::get_double(Option option) const
{
    rule_of<DoubleRule>(row_of(table_, option));
    return std::get<double>(values_[index_of(option)]);
}

const std::string &Options::get_string(Option option) const
{
    const OptionSpec &spec = row_of(table_, option);
    if (!std::holds_alternative<EnumRule>(spec.rule))
    {
        rule_of<StringRule>(spec);
    }
    return std::get<std::string>(values_[index_of(option)]);
}

std::size_t Options::position_of(Option option) const
{
    const EnumRule &rule = rule_of<EnumRule>(row_of(table_, option));
    const std::string &value = std::get<std::string>(values_[index_of(option)]);
    const auto found = std::ranges::find(rule.values, value);
    return static_cast<std::size_t>(found - rule.values.begin());
}

}
