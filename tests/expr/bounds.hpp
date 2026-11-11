#pragma once

#include "expr/interval.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>

namespace biyos::test
{

inline std::string interval_text(Interval interval)
{
    if (interval.is_empty())
    {
        return "empty";
    }
    std::ostringstream out;
    out.precision(17);
    out << "[" << interval.lower() << ", " << interval.upper() << "]";
    return out.str();
}

inline testing::AssertionResult holds(Interval found, double lower, double upper)
{
    if (!found.is_empty() && found.lower() <= lower && upper <= found.upper())
    {
        return testing::AssertionSuccess();
    }
    const std::string wanted = interval_text(Interval(lower, upper));
    return testing::AssertionFailure() << interval_text(found) << " misses part of " << wanted;
}

inline testing::AssertionResult within(Interval found, double lower, double upper)
{
    if (found.is_empty() || (lower <= found.lower() && found.upper() <= upper))
    {
        return testing::AssertionSuccess();
    }
    const std::string bound = interval_text(Interval(lower, upper));
    return testing::AssertionFailure() << interval_text(found) << " leaves " << bound;
}

inline testing::AssertionResult exact(Interval found, double lower, double upper)
{
    const testing::AssertionResult held = holds(found, lower, upper);
    return held ? within(found, lower, upper) : held;
}

}
