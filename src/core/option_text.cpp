#include "core/option_text.hpp"

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

std::string joined(std::span<const std::string_view> names)
{
    std::string text;
    for (const std::string_view name : names)
    {
        if (!text.empty())
        {
            text += ", ";
        }
        text += name;
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

std::string_view noun(const OptionSpec &spec)
{
    return std::visit([](const auto &rule) { return noun(rule); }, spec.rule);
}

}
