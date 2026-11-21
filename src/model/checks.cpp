#include "model/checks.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

[[noreturn]] void unknown(std::string_view noun, Index index)
{
    refuse(noun, index, "unknown");
}

}

std::string item(std::string_view noun, Index index)
{
    return std::string(noun) + " " + text_of(index);
}

void refuse(std::string_view noun, Index index, const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, item(noun, index), reason);
}

std::string_view bound_fault(double lower, double upper) noexcept
{
    if (std::isnan(lower))
    {
        return "lower bound is NaN";
    }
    if (std::isnan(upper))
    {
        return "upper bound is NaN";
    }
    if (std::isinf(lower) && !std::signbit(lower))
    {
        return "lower bound is infinity";
    }
    if (std::isinf(upper) && std::signbit(upper))
    {
        return "upper bound is minus infinity";
    }
    return "";
}

std::string_view value_fault(double value) noexcept
{
    if (std::isnan(value))
    {
        return "is NaN";
    }
    if (std::isinf(value))
    {
        return std::signbit(value) ? "is minus infinity" : "is infinity";
    }
    return "";
}

void check_index(std::string_view noun, Index index, Index total)
{
    if (index < 0 || index >= total)
    {
        unknown(noun, index);
    }
}

void check_span(std::string_view noun, Index start, Index size, Index total)
{
    if (size < 0)
    {
        const std::string reason = text_of(size) + " is negative";
        throw Error(ErrorKind::InvalidArgument, "argument 'n'", reason);
    }
    if (start < 0 || start > total)
    {
        unknown(noun, start);
    }
    if (size > total - start)
    {
        unknown(noun, total);
    }
}

void check_function(std::string_view noun, Index index, Index function, Index functions)
{
    if (function < 0 || function >= functions)
    {
        refuse(noun, index, item("function", function) + " is unknown");
    }
}

void check_functions(std::span<const Index> functions, Index held, std::string_view argument)
{
    for (const Index function : functions)
    {
        check_index("function", function, held);
    }
    std::vector<Index> sorted(functions.begin(), functions.end());
    std::ranges::sort(sorted);
    const auto repeat = std::ranges::adjacent_find(sorted);
    if (repeat != sorted.end())
    {
        const std::string reason = item("function", *repeat) + " is repeated";
        throw Error(ErrorKind::InvalidArgument, "argument '" + std::string(argument) + "'", reason);
    }
}

}
