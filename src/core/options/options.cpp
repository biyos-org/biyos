#include "core/options/options.hpp"

#include "core/error.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/options/checks.hpp"
#include "core/options/table.hpp"
#include "core/options/value.hpp"
#include "core/version.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
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

Real default_of(const DoubleRule &rule)
{
    return rule.default_value;
}

std::string_view default_of(const StringRule &rule)
{
    return rule.default_value;
}

int default_of(const EnumRule &rule)
{
    return rule.default_value;
}

Masks default_of(const FlagsRule &)
{
    return Masks{};
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

Options::Options(Env &env)
    : table_(option_table()), env_(env), values_(defaults()),
      sources_(table_.size(), OptionSource::Default)
{
}

Options::Options(std::span<const OptionSpec> table, Env &env)
    : table_(checked(table)), env_(env), values_(defaults()),
      sources_(table_.size(), OptionSource::Default)
{
}

std::vector<OptionValue> Options::defaults()
{
    std::vector<OptionValue> values;
    values.reserve(table_.size());
    const auto value_of = [](const auto &rule) { return OptionValue(default_of(rule)); };
    for (const OptionSpec &spec : table_)
    {
        values.push_back(kept(strings_, std::visit(value_of, spec.rule)));
    }
    return values;
}

void Options::set_bool(Option option, bool value)
{
    store(option, value);
}

void Options::set_int(Option option, int value)
{
    store(option, value);
}

void Options::set_double(Option option, Real value)
{
    store(option, value);
}

void Options::set_string(Option option, std::string_view value)
{
    store(option, value);
}

void Options::set_flags(Option option, Masks value)
{
    store(option, value);
}

void Options::store(Option option, const OptionValue &value)
{
    const OptionSpec &spec = checked_row(table_, option, value);
    values_[index_of(option)] = kept(strings_, value);
    sources_[index_of(option)] = OptionSource::Environment;
    warn_deprecated(env_.stream(), table_, spec);
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

Real Options::get_double(Option option) const
{
    const OptionSpec &spec = row_of(table_, option);
    return double_of(spec, values_[index_of(option)]);
}

std::string_view Options::get_string(Option option) const
{
    const OptionSpec &spec = row_of(table_, option);
    return string_of(spec, values_[index_of(option)]);
}

Masks Options::get_flags(Option option) const
{
    const OptionSpec &spec = row_of(table_, option);
    return flags_of(spec, values_[index_of(option)]);
}

const OptionValue &Options::value_of(Option option) const
{
    row_of(table_, option);
    return values_[index_of(option)];
}

OptionSource Options::source_of(Option option) const
{
    row_of(table_, option);
    return sources_[index_of(option)];
}

std::span<const OptionSpec> Options::table() const noexcept
{
    return table_;
}

int Options::enum_value(Option option) const
{
    const OptionSpec &spec = row_of(table_, option);
    return enum_of(spec, values_[index_of(option)]);
}

}
