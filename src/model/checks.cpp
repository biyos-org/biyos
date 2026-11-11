#include "model/checks.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"

#include <cmath>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

[[noreturn]] void unknown(std::string_view noun, Index index)
{
    throw Error(ErrorKind::InvalidArgument, item(noun, index), "unknown");
}

}

std::string item(std::string_view noun, Index index)
{
    return std::string(noun) + " " + text_of(index);
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

void check_index(std::string_view noun, Index index, Index size)
{
    if (index < 0 || index >= size)
    {
        unknown(noun, index);
    }
}

void check_span(std::string_view noun, Index first, Index count, Index size)
{
    if (count < 0)
    {
        const std::string reason = text_of(count) + " is negative";
        throw Error(ErrorKind::InvalidArgument, "argument 'count'", reason);
    }
    if (first < 0 || first > size)
    {
        unknown(noun, first);
    }
    if (count > size - first)
    {
        unknown(noun, size);
    }
}

}
