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

Option find_opt(std::string_view name)
{
    const std::span<const OptionSpec> table = opt_table();
    const auto spec = std::ranges::find(table, name, &OptionSpec::name);
    if (spec == table.end())
    {
        throw Error(ErrorKind::InvalidArgument, subject(name), "unknown");
    }
    return spec->id;
}

Option find_opt(int id)
{
    if (id < 1 || static_cast<std::size_t>(id) > opt_table().size())
    {
        throw Error(ErrorKind::InvalidArgument, "option " + text_of(id), "unknown");
    }
    return static_cast<Option>(id);
}

std::string_view name_of(Option opt)
{
    return row_of(opt_table(), opt).name;
}

OptionKind kind_of(Option opt)
{
    const OptionSpec &spec = row_of(opt_table(), opt);
    return std::visit([](const auto &rule) { return kind_of(rule); }, spec.rule);
}

Versions versions_of(Option opt)
{
    return row_of(opt_table(), opt).versions;
}

}
