#include "core/options.hpp"

#include "core/error.hpp"
#include "core/option_table.hpp"
#include "core/option_text.hpp"

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

}

Options::Options()
{
    const std::span<const OptionSpec> table = option_table();
    values_.reserve(table.size());
    const auto value_of = [](const auto &rule) { return Value(default_of(rule)); };
    for (const OptionSpec &spec : table)
    {
        values_.push_back(std::visit(value_of, spec.rule));
    }
}

void Options::set_bool(Option option, bool value)
{
    rule_of<BoolRule>(spec_of(option));
    std::get<bool>(values_[index_of(option)]) = value;
}

void Options::set_int(Option option, int value)
{
    const OptionSpec &spec = spec_of(option);
    check_range(spec, rule_of<IntRule>(spec), value);
    std::get<int>(values_[index_of(option)]) = value;
}

void Options::set_double(Option option, double value)
{
    const OptionSpec &spec = spec_of(option);
    check_range(spec, rule_of<DoubleRule>(spec), value);
    std::get<double>(values_[index_of(option)]) = value;
}

void Options::set_string(Option option, std::string_view value)
{
    const OptionSpec &spec = spec_of(option);
    if (const EnumRule *rule = std::get_if<EnumRule>(&spec.rule))
    {
        check_value(spec, *rule, value);
    }
    else
    {
        rule_of<StringRule>(spec);
    }
    std::get<std::string>(values_[index_of(option)]).assign(value);
}

bool Options::get_bool(Option option) const
{
    rule_of<BoolRule>(spec_of(option));
    return std::get<bool>(values_[index_of(option)]);
}

int Options::get_int(Option option) const
{
    rule_of<IntRule>(spec_of(option));
    return std::get<int>(values_[index_of(option)]);
}

double Options::get_double(Option option) const
{
    rule_of<DoubleRule>(spec_of(option));
    return std::get<double>(values_[index_of(option)]);
}

const std::string &Options::get_string(Option option) const
{
    const OptionSpec &spec = spec_of(option);
    if (!std::holds_alternative<EnumRule>(spec.rule))
    {
        rule_of<StringRule>(spec);
    }
    return std::get<std::string>(values_[index_of(option)]);
}

std::size_t Options::position_of(Option option) const
{
    const EnumRule &rule = rule_of<EnumRule>(spec_of(option));
    const std::string &value = std::get<std::string>(values_[index_of(option)]);
    const auto found = std::ranges::find(rule.values, value);
    return static_cast<std::size_t>(found - rule.values.begin());
}

}
