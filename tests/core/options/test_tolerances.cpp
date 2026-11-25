#include "core/events/events.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "core/options/settings.hpp"
#include "core/timing/clock.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace
{

using biyos::Option;
using biyos::Real;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kNan = std::numeric_limits<Real>::quiet_NaN();
constexpr Real kLargest = (std::numeric_limits<Real>::max)();

biyos::Tolerances defaults()
{
    biyos::FakeClock clock;
    biyos::Events events(clock);
    return biyos::Tolerances(biyos::Lookup(biyos::Options(events)));
}

biyos::Tolerances relative(Real rel)
{
    biyos::FakeClock clock;
    biyos::Events events(clock);
    biyos::Options options(events);
    options.set_double(Option::FeasAbs, 0.0);
    options.set_double(Option::FeasRel, rel);
    return biyos::Tolerances(biyos::Lookup(options));
}

biyos::Tolerances uniform(Real value)
{
    biyos::FakeClock clock;
    biyos::Events events(clock);
    biyos::Options options(events);
    for (const Option option : {Option::FeasAbs, Option::FeasRel, Option::IntTol, Option::ZeroTol})
    {
        options.set_double(option, value);
    }
    return biyos::Tolerances(biyos::Lookup(options));
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

    for (const Real x : {-kInfinity, -kLargest, 0.0, kLargest, kInfinity})
    {
        EXPECT_TRUE(tolerances.satisfies_lower(x, -kInfinity));
        EXPECT_TRUE(tolerances.satisfies_upper(x, kInfinity));
    }
    for (const Real x : {-kLargest, 0.0, kLargest})
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
        for (const Real bound : {-kInfinity, 0.0, kInfinity})
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
    options.set_double(Option::FeasAbs, 1e-2);
    options.set_double(Option::FeasRel, 1e-4);
    options.set_double(Option::IntTol, 0.1);
    options.set_double(Option::ZeroTol, 1e-3);
    const biyos::Tolerances tolerances{biyos::Lookup(options)};

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
    const Real above = std::nextafter(1.0, 2.0);
    EXPECT_TRUE(tolerances.is_zero(0.0));
    EXPECT_FALSE(tolerances.is_zero(std::numeric_limits<Real>::denorm_min()));
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
    const biyos::Tolerances tolerances{biyos::Lookup(options)};
    options.set_double(Option::ZeroTol, 1.0);
    options.set_double(Option::FeasAbs, 1.0);
    options.set_double(Option::IntTol, 0.5);
    EXPECT_FALSE(tolerances.is_zero(0.5));
    EXPECT_FALSE(tolerances.approx_equal(0.0, 0.5));
    EXPECT_FALSE(tolerances.is_integral(0.25));
}

TEST(ToleranceTest, Model)
{
    biyos::FakeClock clock;
    biyos::Events events(clock);
    biyos::Options options(events);
    biyos::Settings settings;
    settings.set_double(Option::ZeroTol, 1e-3, events);
    settings.set_double(Option::IntTol, 0.0, events);
    settings.set_double(Option::FeasAbs, 0.5, events);
    settings.set_double(Option::FeasRel, 0.0, events);
    const biyos::Tolerances model(biyos::Lookup(settings, options));
    const biyos::Tolerances shared{biyos::Lookup(options)};
    EXPECT_TRUE(model.is_zero(5e-4));
    EXPECT_FALSE(shared.is_zero(5e-4));
    EXPECT_FALSE(model.is_integral(1.0 + 1e-6));
    EXPECT_TRUE(shared.is_integral(1.0 + 1e-6));
    EXPECT_TRUE(model.satisfies_lower(1.6, 2.0));
    EXPECT_FALSE(shared.satisfies_lower(1.6, 2.0));
    EXPECT_TRUE(model.approx_equal(1e8, 1e8 + 0.25));
    EXPECT_TRUE(shared.approx_equal(1e8, 1e8 + 0.25));
    EXPECT_FALSE(model.approx_equal(1e8, 1e8 + 1.0));
    EXPECT_TRUE(shared.approx_equal(1e8, 1e8 + 1.0));
}
