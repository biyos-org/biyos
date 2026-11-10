#include "core/clock.hpp"
#include "core/events.hpp"
#include "core/options.hpp"
#include "core/tolerances.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace
{

using biyos::Option;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kLargest = (std::numeric_limits<double>::max)();

biyos::Tolerances defaults()
{
    biyos::FakeClock clock;
    biyos::Events events(clock);
    return biyos::Tolerances(biyos::Options(events));
}

biyos::Tolerances relative(double rel)
{
    biyos::FakeClock clock;
    biyos::Events events(clock);
    biyos::Options options(events);
    options.set_double(Option::FeasibilityAbs, 0.0);
    options.set_double(Option::FeasibilityRel, rel);
    return biyos::Tolerances(options);
}

biyos::Tolerances uniform(double value)
{
    biyos::FakeClock clock;
    biyos::Events events(clock);
    biyos::Options options(events);
    for (const Option option :
         {Option::FeasibilityAbs, Option::FeasibilityRel, Option::IntegralityTol, Option::ZeroTol})
    {
        options.set_double(option, value);
    }
    return biyos::Tolerances(options);
}

}

TEST(ToleranceTest, NearZero)
{
    const biyos::Tolerances tolerances = defaults();
    EXPECT_TRUE(tolerances.is_zero(0.0));
    EXPECT_TRUE(tolerances.is_zero(-0.0));
    EXPECT_TRUE(tolerances.is_zero(5e-10));
    EXPECT_TRUE(tolerances.is_zero(-5e-10));
    EXPECT_FALSE(tolerances.is_zero(2e-9));
    EXPECT_FALSE(tolerances.is_zero(-2e-9));

    EXPECT_TRUE(tolerances.approx_equal(0.0, 5e-7));
    EXPECT_TRUE(tolerances.approx_equal(-4e-7, 4e-7));
    EXPECT_TRUE(tolerances.approx_equal(1e-12, 1e-7));
    EXPECT_FALSE(tolerances.approx_equal(0.0, 2e-6));
    EXPECT_FALSE(tolerances.approx_equal(-1e-6, 1e-6));

    EXPECT_TRUE(tolerances.satisfies_lower(1.0, 0.0));
    EXPECT_TRUE(tolerances.satisfies_lower(-5e-7, 0.0));
    EXPECT_FALSE(tolerances.satisfies_lower(-2e-6, 0.0));
    EXPECT_TRUE(tolerances.satisfies_upper(-1.0, 0.0));
    EXPECT_TRUE(tolerances.satisfies_upper(5e-7, 0.0));
    EXPECT_FALSE(tolerances.satisfies_upper(2e-6, 0.0));

    EXPECT_TRUE(tolerances.is_integral(0.0));
    EXPECT_TRUE(tolerances.is_integral(5e-6));
    EXPECT_TRUE(tolerances.is_integral(-5e-6));
    EXPECT_TRUE(tolerances.is_integral(1.0 - 5e-6));
    EXPECT_FALSE(tolerances.is_integral(2e-5));
    EXPECT_FALSE(tolerances.is_integral(0.5));
}

TEST(ToleranceTest, LargeMagnitude)
{
    const biyos::Tolerances tolerances = defaults();
    EXPECT_FALSE(tolerances.is_zero(1e9));
    EXPECT_FALSE(tolerances.is_zero(-kLargest));

    EXPECT_TRUE(tolerances.approx_equal(1e9, 1e9 + 500.0));
    EXPECT_TRUE(tolerances.approx_equal(-1e9 - 500.0, -1e9));
    EXPECT_FALSE(tolerances.approx_equal(1e9, 1e9 + 2000.0));
    EXPECT_TRUE(tolerances.approx_equal(kLargest, kLargest));
    EXPECT_FALSE(tolerances.approx_equal(kLargest, -kLargest));

    EXPECT_TRUE(tolerances.satisfies_lower(1e9 - 500.0, 1e9));
    EXPECT_FALSE(tolerances.satisfies_lower(1e9 - 2000.0, 1e9));
    EXPECT_TRUE(tolerances.satisfies_upper(-1e9 + 500.0, -1e9));
    EXPECT_FALSE(tolerances.satisfies_upper(-1e9 + 2000.0, -1e9));
    EXPECT_FALSE(tolerances.satisfies_lower(-kLargest, kLargest));
    EXPECT_FALSE(tolerances.satisfies_upper(kLargest, -kLargest));

    EXPECT_TRUE(tolerances.is_integral(1e15));
    EXPECT_TRUE(tolerances.is_integral(-kLargest));
    EXPECT_FALSE(tolerances.is_integral(1e15 + 0.5));
    EXPECT_FALSE(tolerances.is_integral(-1e15 - 0.5));
}

TEST(ToleranceTest, Infinity)
{
    const biyos::Tolerances tolerances = defaults();
    EXPECT_FALSE(tolerances.is_zero(kInfinity));
    EXPECT_FALSE(tolerances.is_zero(-kInfinity));

    EXPECT_TRUE(tolerances.approx_equal(kInfinity, kInfinity));
    EXPECT_TRUE(tolerances.approx_equal(-kInfinity, -kInfinity));
    EXPECT_FALSE(tolerances.approx_equal(kInfinity, -kInfinity));
    EXPECT_FALSE(tolerances.approx_equal(kInfinity, kLargest));
    EXPECT_FALSE(tolerances.approx_equal(0.0, -kInfinity));

    for (const double x : {-kInfinity, -kLargest, 0.0, kLargest, kInfinity})
    {
        EXPECT_TRUE(tolerances.satisfies_lower(x, -kInfinity));
        EXPECT_TRUE(tolerances.satisfies_upper(x, kInfinity));
    }
    for (const double x : {-kLargest, 0.0, kLargest})
    {
        EXPECT_FALSE(tolerances.satisfies_lower(x, kInfinity));
        EXPECT_FALSE(tolerances.satisfies_upper(x, -kInfinity));
    }
    EXPECT_FALSE(tolerances.satisfies_lower(-kInfinity, kInfinity));
    EXPECT_FALSE(tolerances.satisfies_upper(kInfinity, -kInfinity));
    EXPECT_TRUE(tolerances.satisfies_lower(kInfinity, kInfinity));
    EXPECT_TRUE(tolerances.satisfies_upper(-kInfinity, -kInfinity));
    EXPECT_TRUE(tolerances.satisfies_lower(kInfinity, 0.0));
    EXPECT_FALSE(tolerances.satisfies_lower(-kInfinity, 0.0));
    EXPECT_TRUE(tolerances.satisfies_upper(-kInfinity, 0.0));
    EXPECT_FALSE(tolerances.satisfies_upper(kInfinity, 0.0));

    EXPECT_FALSE(tolerances.is_integral(kInfinity));
    EXPECT_FALSE(tolerances.is_integral(-kInfinity));

    const biyos::Tolerances loose = uniform(kInfinity);
    EXPECT_FALSE(loose.satisfies_lower(kLargest, kInfinity));
    EXPECT_FALSE(loose.satisfies_upper(-kLargest, -kInfinity));
    EXPECT_FALSE(loose.satisfies_lower(-kInfinity, 0.0));
    EXPECT_FALSE(loose.satisfies_lower(-kInfinity, -kLargest));
    EXPECT_FALSE(loose.satisfies_upper(kInfinity, 0.0));
    EXPECT_FALSE(loose.satisfies_upper(kInfinity, kLargest));
    EXPECT_TRUE(loose.satisfies_lower(kInfinity, 0.0));
    EXPECT_TRUE(loose.satisfies_upper(-kInfinity, 0.0));
    EXPECT_FALSE(loose.approx_equal(-kInfinity, 0.0));
}

TEST(ToleranceTest, NanFails)
{
    for (const biyos::Tolerances &tolerances : {defaults(), uniform(kInfinity)})
    {
        EXPECT_FALSE(tolerances.is_zero(kNan));
        EXPECT_FALSE(tolerances.approx_equal(kNan, kNan));
        EXPECT_FALSE(tolerances.approx_equal(kNan, 0.0));
        EXPECT_FALSE(tolerances.approx_equal(0.0, kNan));
        EXPECT_FALSE(tolerances.approx_equal(kNan, kInfinity));
        EXPECT_FALSE(tolerances.approx_equal(-kInfinity, kNan));
        for (const double bound : {-kInfinity, 0.0, kInfinity})
        {
            EXPECT_FALSE(tolerances.satisfies_lower(kNan, bound));
            EXPECT_FALSE(tolerances.satisfies_upper(kNan, bound));
        }
        EXPECT_FALSE(tolerances.satisfies_lower(0.0, kNan));
        EXPECT_FALSE(tolerances.satisfies_upper(0.0, kNan));
        EXPECT_FALSE(tolerances.is_integral(kNan));
    }
}

TEST(ToleranceTest, ReadsOptions)
{
    biyos::FakeClock clock;
    biyos::Events events(clock);
    biyos::Options options(events);
    options.set_double(Option::FeasibilityAbs, 1e-2);
    options.set_double(Option::FeasibilityRel, 1e-4);
    options.set_double(Option::IntegralityTol, 0.1);
    options.set_double(Option::ZeroTol, 1e-3);
    const biyos::Tolerances tolerances(options);

    EXPECT_TRUE(tolerances.is_zero(5e-4));
    EXPECT_FALSE(tolerances.is_zero(2e-3));

    EXPECT_TRUE(tolerances.approx_equal(1.0, 1.005));
    EXPECT_FALSE(tolerances.approx_equal(1.0, 1.02));
    EXPECT_TRUE(tolerances.approx_equal(1e3, 1e3 + 0.05));
    EXPECT_FALSE(tolerances.approx_equal(1e3, 1e3 + 0.2));

    EXPECT_TRUE(tolerances.satisfies_lower(1.0 - 0.005, 1.0));
    EXPECT_FALSE(tolerances.satisfies_lower(1.0 - 0.02, 1.0));
    EXPECT_TRUE(tolerances.satisfies_upper(1e3 + 0.05, 1e3));
    EXPECT_FALSE(tolerances.satisfies_upper(1e3 + 0.2, 1e3));

    EXPECT_TRUE(tolerances.is_integral(2.05));
    EXPECT_FALSE(tolerances.is_integral(2.2));
}

TEST(ToleranceTest, Scale)
{
    const biyos::Tolerances fine = relative(1e-3);
    EXPECT_TRUE(fine.approx_equal(0.0, 5e-4));
    EXPECT_TRUE(fine.approx_equal(5e-4, 0.0));
    EXPECT_FALSE(fine.approx_equal(0.0, 2e-3));
    EXPECT_TRUE(fine.satisfies_lower(-5e-4, 0.0));
    EXPECT_FALSE(fine.satisfies_lower(-2e-3, 0.0));
    EXPECT_TRUE(fine.satisfies_upper(5e-4, 0.0));
    EXPECT_FALSE(fine.satisfies_upper(2e-3, 0.0));

    const biyos::Tolerances coarse = relative(0.5);
    EXPECT_TRUE(coarse.approx_equal(1.0, 1.9));
    EXPECT_TRUE(coarse.approx_equal(1.9, 1.0));
    EXPECT_FALSE(coarse.approx_equal(1.0, 2.1));
    EXPECT_TRUE(coarse.satisfies_lower(-1.4, -1.0));
    EXPECT_FALSE(coarse.satisfies_lower(-1.8, -1.0));
    EXPECT_TRUE(coarse.satisfies_upper(1.4, 1.0));
    EXPECT_FALSE(coarse.satisfies_upper(1.8, 1.0));
}

TEST(ToleranceTest, Overflow)
{
    const biyos::Tolerances wide = relative(1.5);
    EXPECT_FALSE(wide.approx_equal(kLargest, -kLargest));
    EXPECT_FALSE(wide.satisfies_lower(-kLargest, kLargest));
    EXPECT_FALSE(wide.satisfies_upper(kLargest, -kLargest));
    EXPECT_TRUE(wide.approx_equal(kLargest, kLargest / 4));

    const biyos::Tolerances wider = relative(2.5);
    EXPECT_TRUE(wider.approx_equal(kLargest, -kLargest));
    EXPECT_TRUE(wider.satisfies_lower(-kLargest, kLargest));
    EXPECT_TRUE(wider.satisfies_upper(kLargest, -kLargest));

    const biyos::Tolerances loose = uniform(kInfinity);
    EXPECT_TRUE(loose.approx_equal(kLargest, -kLargest));
    EXPECT_TRUE(loose.satisfies_lower(-kLargest, kLargest));
}

TEST(ToleranceTest, Exact)
{
    const biyos::Tolerances tolerances = uniform(0.0);
    const double above = std::nextafter(1.0, 2.0);
    EXPECT_TRUE(tolerances.is_zero(0.0));
    EXPECT_FALSE(tolerances.is_zero(std::numeric_limits<double>::denorm_min()));
    EXPECT_TRUE(tolerances.approx_equal(1.0, 1.0));
    EXPECT_FALSE(tolerances.approx_equal(1.0, above));
    EXPECT_TRUE(tolerances.satisfies_lower(1.0, 1.0));
    EXPECT_FALSE(tolerances.satisfies_lower(1.0, above));
    EXPECT_TRUE(tolerances.satisfies_upper(1.0, 1.0));
    EXPECT_FALSE(tolerances.satisfies_upper(above, 1.0));
    EXPECT_TRUE(tolerances.is_integral(3.0));
    EXPECT_FALSE(tolerances.is_integral(std::nextafter(3.0, 4.0)));
}

TEST(ToleranceTest, Immutable)
{
    biyos::FakeClock clock;
    biyos::Events events(clock);
    biyos::Options options(events);
    const biyos::Tolerances tolerances(options);
    options.set_double(Option::ZeroTol, 1.0);
    options.set_double(Option::FeasibilityAbs, 1.0);
    options.set_double(Option::IntegralityTol, 0.5);
    EXPECT_FALSE(tolerances.is_zero(0.5));
    EXPECT_FALSE(tolerances.approx_equal(0.0, 0.5));
    EXPECT_FALSE(tolerances.is_integral(0.25));
}
