#pragma once

#include "core/index.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"
#include "model/overrides.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::test
{

struct ToleranceCalls
{
    std::string_view noun;
    Override (Model::*read)(Index) const;
    void (Model::*set)(std::span<const Index>, std::span<const double>, std::span<const double>);
    void (Model::*remove)(std::span<const Index>);
};

inline constexpr double kUnknown = std::numeric_limits<double>::quiet_NaN();

inline void expect_part(double found, double wanted)
{
    EXPECT_EQ(std::bit_cast<std::uint64_t>(found), std::bit_cast<std::uint64_t>(wanted));
}

inline void expect_tolerance(const Override &found, double absolute, double relative)
{
    expect_part(found.absolute, absolute);
    expect_part(found.relative, relative);
}

inline void set_tolerances(Model &model,
                           const ToleranceCalls &calls,
                           const std::vector<Index> &indices,
                           const std::vector<double> &absolute,
                           const std::vector<double> &relative)
{
    (model.*calls.set)(indices, absolute, relative);
}

inline std::string tolerance_refusal(Model &model,
                                     const ToleranceCalls &calls,
                                     const std::vector<Index> &indices,
                                     const std::vector<double> &absolute,
                                     const std::vector<double> &relative)
{
    return refusal([&] { set_tolerances(model, calls, indices, absolute, relative); });
}

inline std::string refused(const ToleranceCalls &calls, Index index, std::string_view reason)
{
    return "invalid argument: " + std::string(calls.noun) + " " + std::to_string(index) + ": " +
           std::string(reason);
}

inline void expect_own(Model model, const ToleranceCalls &calls)
{
    set_tolerances(model, calls, {2, 0}, {1e-6, 0.0}, {1e-3, 2.0});
    expect_tolerance((model.*calls.read)(0), 0.0, 2.0);
    expect_tolerance((model.*calls.read)(1), kUnknown, kUnknown);
    expect_tolerance((model.*calls.read)(2), 1e-6, 1e-3);
    set_tolerances(model, calls, {1}, {0.5}, {});
    expect_tolerance((model.*calls.read)(1), 0.5, kUnknown);
    set_tolerances(model, calls, {1, 2}, {}, {0.25, 0.125});
    expect_tolerance((model.*calls.read)(1), 0.5, 0.25);
    expect_tolerance((model.*calls.read)(2), 1e-6, 0.125);
    EXPECT_EQ(refusal([&] { (model.*calls.read)(3); }), refused(calls, 3, "unknown"));
}

inline void expect_refused(Model model, const ToleranceCalls &calls)
{
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    set_tolerances(model, calls, {0}, {1.0}, {2.0});
    EXPECT_EQ(tolerance_refusal(model, calls, {0, 1}, {3.0, -0.5}, {}),
              refused(calls, 1, "absolute tolerance -0.5 is negative"));
    EXPECT_EQ(tolerance_refusal(model, calls, {0, 1}, {}, {3.0, kUnknown}),
              refused(calls, 1, "relative tolerance is NaN"));
    EXPECT_EQ(tolerance_refusal(model, calls, {0, 1}, {3.0, kInfinity}, {}),
              refused(calls, 1, "absolute tolerance is infinity"));
    EXPECT_EQ(tolerance_refusal(model, calls, {0, 2}, {3.0, 1.0}, {}),
              refused(calls, 2, "unknown"));
    expect_tolerance((model.*calls.read)(0), 1.0, 2.0);
    expect_tolerance((model.*calls.read)(1), kUnknown, kUnknown);
}

inline void expect_deleted(Model model, const ToleranceCalls &calls)
{
    set_tolerances(model, calls, {0, 1}, {1.0, 2.0}, {3.0, 4.0});
    const std::vector<Index> beyond{1, 2};
    EXPECT_EQ(refusal([&] { (model.*calls.remove)(beyond); }), refused(calls, 2, "unknown"));
    expect_tolerance((model.*calls.read)(1), 2.0, 4.0);
    (model.*calls.remove)(std::vector<Index>{1});
    expect_tolerance((model.*calls.read)(0), 1.0, 3.0);
    expect_tolerance((model.*calls.read)(1), kUnknown, kUnknown);
}

inline void expect_copied(Model model, const ToleranceCalls &calls)
{
    set_tolerances(model, calls, {1}, {0.5}, {0.25});
    const Model copy = model;
    EXPECT_EQ(difference(model, copy), "");
    expect_tolerance((copy.*calls.read)(1), 0.5, 0.25);
    Model changed = model;
    set_tolerances(changed, calls, {1}, {}, {std::nextafter(0.25, 1.0)});
    const std::string record = std::string(calls.noun) + " 1: ";
    EXPECT_EQ(difference(model, changed),
              record + "relative tolerance 0.25 against 0.25000000000000006");
    (changed.*calls.remove)(std::vector<Index>{1});
    EXPECT_EQ(difference(model, changed), record + "absolute tolerance 0.5 against nan");
}

}
