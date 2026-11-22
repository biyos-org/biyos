#include "core/options.hpp"

#include "core/error.hpp"
#include "core/option_checks.hpp"
#include "core/option_table.hpp"
#include "core/option_value.hpp"
#include "core/version.hpp"

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

std::span<const OptionSpec> checked(std::span<const OptionSpec> table)
{
    if (!well_formed(table, version()))
    {
        throw Error(ErrorKind::Internal, "option table", "is not well formed");
    }
    return table;
}

}

Options::Options(Events &events)
    : table_(option_table()), events_(events), values_(defaults(table_))
{
}

Options::Options(std::span<const OptionSpec> table, Events &events)
    : table_(checked(table)), events_(events), values_(defaults(table_))
{
}

std::vector<OptionValue> Options::defaults(std::span<const OptionSpec> table)
{
    std::vector<OptionValue> values;
    values.reserve(table.size());
    const auto value_of = [](const auto &rule) { return OptionValue(default_of(rule)); };
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

void Options::store(const OptionSpec &spec, OptionValue value)
{
    values_[index_of(spec.id)] = std::move(value);
    warn_deprecated(events_, table_, spec);
}

bool Options::get_bool(Option option) const
{
    const OptionSpec &spec = row_of(table_, option);
    return bool_of(spec, values_[index_of(option)]);
}

int Options::get_int(Option option) const
{
    const OptionSpec &spec = row_of(table_, option);
    return int_of(spec, values_[index_of(option)]);
}

double Options::get_double(Option option) const
{
    const OptionSpec &spec = row_of(table_, option);
    return double_of(spec, values_[index_of(option)]);
}

const std::string &Options::get_string(Option option) const
{
    const OptionSpec &spec = row_of(table_, option);
    return string_of(spec, values_[index_of(option)]);
}

std::size_t Options::position(Option option) const
{
    const OptionSpec &spec = row_of(table_, option);
    return position_of(spec, values_[index_of(option)]);
}

}
