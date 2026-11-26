#include "model/compare/fields.hpp"

#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"

#include <initializer_list>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

std::string quoted(const std::string *name)
{
    return name == nullptr ? "none" : "'" + *name + "'";
}

}

std::string compared(std::string_view field, Index a, Index b)
{
    return a == b ? std::string() : against(field, a, b);
}

std::string compared(std::string_view field, Offset a, Offset b)
{
    return a == b ? std::string() : against(field, a, b);
}

std::string compared(std::string_view field, Real a, Real b)
{
    return identical(a, b) ? std::string() : against(field, a, b);
}

std::string compared(std::string_view field, const std::string *a, const std::string *b)
{
    const bool same = a == nullptr || b == nullptr ? a == b : *a == *b;
    return same ? std::string() : std::string(field) + " " + quoted(a) + " against " + quoted(b);
}

std::string first_of(std::initializer_list<std::string> found)
{
    for (const std::string &text : found)
    {
        if (!text.empty())
        {
            return text;
        }
    }
    return "";
}

}
