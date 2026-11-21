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

[[noreturn]] void unknown(std::string_view noun, Index idx)
{
    refuse(noun, idx, "unknown");
}

}

std::string item(std::string_view noun, Index idx)
{
    return std::string(noun) + " " + text_of(idx);
}

void refuse(std::string_view noun, Index idx, const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, item(noun, idx), reason);
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

void check_idx(std::string_view noun, Index idx, Index size)
{
    if (idx < 0 || idx >= size)
    {
        unknown(noun, idx);
    }
}

void check_span(std::string_view noun, Index beg, Index n, Index size)
{
    if (n < 0)
    {
        const std::string reason = text_of(n) + " is negative";
        throw Error(ErrorKind::InvalidArgument, "argument 'n'", reason);
    }
    if (beg < 0 || beg > size)
    {
        unknown(noun, beg);
    }
    if (n > size - beg)
    {
        unknown(noun, size);
    }
}

void check_fun(std::string_view noun, Index idx, Index fun, Index funs)
{
    if (fun < 0 || fun >= funs)
    {
        refuse(noun, idx, item("function", fun) + " is unknown");
    }
}

}
