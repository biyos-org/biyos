#pragma once

#include "core/options.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <variant>

namespace biyos
{

constexpr bool admits(const BoolRule &)
{
    return true;
}

constexpr bool admits(const IntRule &rule)
{
    return rule.lower <= rule.default_value && rule.default_value <= rule.upper;
}

constexpr bool admits(const DoubleRule &rule)
{
    return rule.lower <= rule.default_value && rule.default_value <= rule.upper;
}

constexpr bool admits(const StringRule &)
{
    return true;
}

constexpr bool admits(const EnumRule &rule)
{
    return std::ranges::find(rule.values, rule.default_value) != rule.values.end();
}

constexpr bool admits(const OptionSpec &spec)
{
    return std::visit([](const auto &rule) { return admits(rule); }, spec.rule);
}

constexpr bool well_formed(std::span<const OptionSpec> table)
{
    for (std::size_t i = 0; i < table.size(); ++i)
    {
        const OptionSpec &spec = table[i];
        const bool in_order = static_cast<std::size_t>(spec.id) == i + 1;
        const bool unique = std::ranges::count(table, spec.name, &OptionSpec::name) == 1;
        if (!in_order || !unique || !admits(spec))
        {
            return false;
        }
    }
    return true;
}

}
