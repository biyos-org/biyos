#include "core/options/text.hpp"

#include "core/text.hpp"

#include <bit>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace biyos
{

std::string subject(std::string_view name)
{
    return "option '" + std::string(name) + "'";
}

std::string joined(std::span<const EnumValue> values)
{
    std::string text;
    for (const EnumValue &value : values)
    {
        if (!text.empty())
        {
            text += ", ";
        }
        text += text_of(value.number) + " (" + std::string(value.name) + ")";
    }
    return text;
}

std::string joined(std::span<const FlagPart> parts)
{
    std::string text;
    for (const FlagPart &part : parts)
    {
        if (!text.empty())
        {
            text += ", ";
        }
        text += text_of(std::countr_zero(part.bit)) + " (" + std::string(part.name) + ")";
    }
    return text;
}

std::string bits(Mask mask)
{
    std::string text = std::has_single_bit(mask) ? "bit " : "bits ";
    for (Mask rest = mask; rest != 0; rest &= rest - 1)
    {
        if (rest != mask)
        {
            text += ", ";
        }
        text += text_of(std::countr_zero(rest));
    }
    return text;
}

std::string_view noun(const BoolRule &)
{
    return "a bool";
}

std::string_view noun(const IntRule &)
{
    return "an int";
}

std::string_view noun(const DoubleRule &)
{
    return "a double";
}

std::string_view noun(const StringRule &)
{
    return "a string";
}

std::string_view noun(const EnumRule &)
{
    return "an enum";
}

std::string_view noun(const FlagsRule &)
{
    return "flags";
}

std::string_view noun(const OptionSpec &spec)
{
    return std::visit([](const auto &rule) { return noun(rule); }, spec.rule);
}

std::string dotted(Version version)
{
    return text_of(version.major) + "." + text_of(version.minor) + "." + text_of(version.patch);
}

std::string notice(std::string_view stage, Version version, std::string_view replacement)
{
    const std::string use = ", use '" + std::string(replacement) + "'";
    return std::string(stage) + " in " + dotted(version) + use;
}

}
