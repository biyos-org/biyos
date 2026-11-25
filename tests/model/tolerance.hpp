#pragma once

#include "core/numbers.hpp"
#include "model/attributes/records.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"
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

inline constexpr double kUnknown = std::numeric_limits<double>::quiet_NaN();

inline void expect_part(double found, double wanted)
{
    EXPECT_EQ(std::bit_cast<std::uint64_t>(found), std::bit_cast<std::uint64_t>(wanted));
}

inline void expect_tolerance(const Tolerance &found, double absolute, double relative)
{
    expect_part(found.absolute, absolute);
    expect_part(found.relative, relative);
}

inline void set_tolerances(Model &model,
                           Family family,
                           const std::vector<Index> &indices,
                           const std::vector<double> &absolute,
                           const std::vector<double> &relative)
{
    model.set_tolerances(family, indices, absolute, relative);
}

inline std::string tolerance_refusal(Model &model,
                                     Family family,
                                     const std::vector<Index> &indices,
                                     const std::vector<double> &absolute,
                                     const std::vector<double> &relative)
{
    return refusal([&] { set_tolerances(model, family, indices, absolute, relative); });
}

inline std::string refused(Family family, Index index, std::string_view reason)
{
    return "invalid argument: " + std::string(noun_of(family)) + " " + std::to_string(index) +
           ": " + std::string(reason);
}

inline void expect_own(Model model, Family family)
{
    set_tolerances(model, family, {2, 0}, {1e-6, 0.0}, {1e-3, 2.0});
    expect_tolerance(model.tolerance_of(family, 0), 0.0, 2.0);
    expect_tolerance(model.tolerance_of(family, 1), kUnknown, kUnknown);
    expect_tolerance(model.tolerance_of(family, 2), 1e-6, 1e-3);
    set_tolerances(model, family, {1}, {0.5}, {});
    expect_tolerance(model.tolerance_of(family, 1), 0.5, kUnknown);
    set_tolerances(model, family, {1, 2}, {}, {0.25, 0.125});
    expect_tolerance(model.tolerance_of(family, 1), 0.5, 0.25);
    expect_tolerance(model.tolerance_of(family, 2), 1e-6, 0.125);
    EXPECT_EQ(refusal([&] { model.tolerance_of(family, 3); }), refused(family, 3, "unknown"));
}

inline void expect_refused(Model model, Family family)
{
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    set_tolerances(model, family, {0}, {1.0}, {2.0});
    EXPECT_EQ(tolerance_refusal(model, family, {0, 1}, {3.0, -0.5}, {}),
              refused(family, 1, "absolute tolerance -0.5 is negative"));
    EXPECT_EQ(tolerance_refusal(model, family, {0, 1}, {}, {3.0, kUnknown}),
              refused(family, 1, "relative tolerance is NaN"));
    EXPECT_EQ(tolerance_refusal(model, family, {0, 1}, {3.0, kInfinity}, {}),
              refused(family, 1, "absolute tolerance is infinity"));
    EXPECT_EQ(tolerance_refusal(model, family, {0, 2}, {3.0, 1.0}, {}),
              refused(family, 2, "unknown"));
    expect_tolerance(model.tolerance_of(family, 0), 1.0, 2.0);
    expect_tolerance(model.tolerance_of(family, 1), kUnknown, kUnknown);
}

inline void expect_deleted(Model model, Family family)
{
    set_tolerances(model, family, {0, 1}, {1.0, 2.0}, {3.0, 4.0});
    const std::vector<Index> beyond{1, 2};
    EXPECT_EQ(refusal([&] { model.delete_tolerances(family, beyond); }),
              refused(family, 2, "unknown"));
    expect_tolerance(model.tolerance_of(family, 1), 2.0, 4.0);
    model.delete_tolerances(family, std::vector<Index>{1});
    expect_tolerance(model.tolerance_of(family, 0), 1.0, 3.0);
    expect_tolerance(model.tolerance_of(family, 1), kUnknown, kUnknown);
}

inline void expect_copied(Model model, Family family)
{
    set_tolerances(model, family, {1}, {0.5}, {0.25});
    const Model copy = model;
    EXPECT_EQ(difference(model, copy), "");
    expect_tolerance(copy.tolerance_of(family, 1), 0.5, 0.25);
    Model changed = model;
    set_tolerances(changed, family, {1}, {}, {std::nextafter(0.25, 1.0)});
    const std::string record = std::string(noun_of(family)) + " 1: ";
    EXPECT_EQ(difference(model, changed),
              record + "relative tolerance 0.25 against 0.25000000000000006");
    changed.delete_tolerances(family, std::vector<Index>{1});
    EXPECT_EQ(difference(model, changed), record + "absolute tolerance 0.5 against nan");
}

}
