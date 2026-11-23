#pragma once

#include "core/options.hpp"
#include "core/version.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
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

constexpr bool sequential(std::span<const EnumValue> values)
{
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        const bool unique = std::ranges::count(values, values[i].name, &EnumValue::name) == 1;
        if (values[i].number != static_cast<int>(i) + 1 || !unique)
        {
            return false;
        }
    }
    return true;
}

constexpr bool admits(const EnumRule &rule)
{
    const auto found = std::ranges::find(rule.values, rule.default_value, &EnumValue::number);
    return found != rule.values.end() && sequential(rule.values);
}

constexpr bool sequential(std::span<const FlagPart> parts)
{
    const auto width = static_cast<std::size_t>(std::numeric_limits<Mask>::digits);
    if (parts.size() > width)
    {
        return false;
    }
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        const bool unique = std::ranges::count(parts, parts[i].name, &FlagPart::name) == 1;
        if (parts[i].bit != Mask{1} << i || !unique)
        {
            return false;
        }
    }
    return true;
}

constexpr bool admits(const FlagsRule &rule)
{
    return !rule.parts.empty() && sequential(rule.parts);
}

constexpr bool admits(const OptionSpec &spec)
{
    return std::visit([](const auto &rule) { return admits(rule); }, spec.rule);
}

constexpr bool scoped(Scope scope)
{
    return scope == Scope::Model || scope == Scope::Environment;
}

constexpr bool released(Version version, Version project)
{
    return numbered(version) && Version{} < version && version <= project;
}

constexpr bool follows(std::optional<Version> later, Version earlier, Version project)
{
    return !later || (earlier < *later && released(*later, project));
}

constexpr bool dated(const Versions &versions, Version project)
{
    const bool chained = versions.deprecated || !versions.retired;
    const Version previous = versions.deprecated.value_or(versions.since);
    const bool deprecated = follows(versions.deprecated, versions.since, project);
    const bool retired = follows(versions.retired, previous, project);
    return chained && released(versions.since, project) && deprecated && retired;
}

constexpr bool valid_replacement(std::span<const OptionSpec> table, const OptionSpec &spec)
{
    const bool deprecated = spec.versions.deprecated.has_value();
    if (!deprecated || !spec.replacement)
    {
        return !deprecated && !spec.replacement;
    }
    const std::size_t index = static_cast<std::size_t>(*spec.replacement) - 1;
    return index < table.size() && !table[index].versions.deprecated;
}

constexpr bool well_formed(std::span<const OptionSpec> table, Version project)
{
    for (std::size_t i = 0; i < table.size(); ++i)
    {
        const OptionSpec &spec = table[i];
        const bool in_order = static_cast<std::size_t>(spec.id) == i + 1;
        const bool unique = std::ranges::count(table, spec.name, &OptionSpec::name) == 1;
        const bool versioned = dated(spec.versions, project) && valid_replacement(table, spec);
        if (!in_order || !unique || !scoped(spec.scope) || !admits(spec) || !versioned)
        {
            return false;
        }
    }
    return true;
}

}
