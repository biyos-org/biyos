#include "core/options.hpp"

#include "core/error.hpp"
#include "core/option_table.hpp"
#include "core/option_text.hpp"
#include "core/text.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>
#include <variant>

namespace biyos
{

namespace
{

constexpr OptionKind kind_of(const BoolRule &)
{
    return OptionKind::Bool;
}

constexpr OptionKind kind_of(const IntRule &)
{
    return OptionKind::Int;
}

constexpr OptionKind kind_of(const DoubleRule &)
{
    return OptionKind::Double;
}

constexpr OptionKind kind_of(const StringRule &)
{
    return OptionKind::String;
}

constexpr OptionKind kind_of(const EnumRule &)
{
    return OptionKind::Enum;
}

}

Option find_option(std::string_view name)
{
    const std::span<const OptionSpec> table = option_table();
    const auto spec = std::ranges::find(table, name, &OptionSpec::name);
    if (spec == table.end())
    {
        throw Error(ErrorKind::InvalidArgument, subject(name), "unknown");
    }
    return spec->id;
}

Option find_option(int id)
{
    if (id < 1 || static_cast<std::size_t>(id) > option_table().size())
    {
        throw Error(ErrorKind::InvalidArgument, "option " + text_of(id), "unknown");
    }
    return static_cast<Option>(id);
}

std::string_view name_of(Option option)
{
    return row_of(option_table(), option).name;
}

OptionKind kind_of(Option option)
{
    const OptionSpec &spec = row_of(option_table(), option);
    return std::visit([](const auto &rule) { return kind_of(rule); }, spec.rule);
}

Versions versions_of(Option option)
{
    return row_of(option_table(), option).versions;
}

}
