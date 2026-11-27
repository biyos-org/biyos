#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"
#include "refusal.hpp"
#include "sample.hpp"
#include "tolerance.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <limits>
#include <numeric>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::Bits;
using biyos::ConeKind;
using biyos::Constraint;
using biyos::difference;
using biyos::Error;
using biyos::Family;
using biyos::Index;
using biyos::Model;
using biyos::Offset;
using biyos::Real;
using biyos::test::Cones;
using biyos::test::expect_copied;
using biyos::test::expect_deleted;
using biyos::test::expect_own;
using biyos::test::expect_refused;
using biyos::test::refusal;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kNan = std::numeric_limits<Real>::quiet_NaN();

struct Shape
{
    ConeKind kind;
    Index dimension;
    std::size_t parameters;
    std::string fault;
};

const std::vector<Shape> kShapes{
    {ConeKind::Nonnegative, 1, 0, ""},
    {ConeKind::Nonnegative, 4, 0, ""},
    {ConeKind::Nonnegative, 0, 0, "has no member"},
    {ConeKind::SecondOrder, 1, 0, ""},
    {ConeKind::SecondOrder, 3, 0, ""},
    {ConeKind::SecondOrder, 0, 0, "has no member"},
    {ConeKind::Rotated, 2, 0, ""},
    {ConeKind::Rotated, 5, 0, ""},
    {ConeKind::Rotated, 1, 0, "dimension 1 is below 2"},
    {ConeKind::Exponential, 3, 0, ""},
    {ConeKind::Exponential, 2, 0, "dimension 2 is not 3"},
    {ConeKind::Exponential, 4, 0, "dimension 4 is not 3"},
    {ConeKind::DualExponential, 3, 0, ""},
    {ConeKind::DualExponential, 1, 0, "dimension 1 is not 3"},
    {ConeKind::Power, 2, 2, ""},
    {ConeKind::Power, 3, 1, ""},
    {ConeKind::Power, 1, 2, "dimension 1 is below its 2 parameters"},
    {ConeKind::DualPower, 2, 1, ""},
    {ConeKind::DualPower, 2, 3, "dimension 2 is below its 3 parameters"},
    {ConeKind::Semidefinite, 1, 0, ""},
    {ConeKind::Semidefinite, 3, 0, ""},
    {ConeKind::Semidefinite, 10, 0, ""},
    {ConeKind::Semidefinite, 4, 0, "dimension 4 is not n(n+1)/2"},
    {ConeKind::Semidefinite, 0, 0, "has no member"},
};

Model with_functions(std::size_t n)
{
    Model model;
    const std::vector<Real> constants(n, 0.0);
    const std::vector<Offset> starts(n + 1, 0);
    model.add_functions(constants, starts, {});
    return model;
}

std::vector<Index> first(Index n)
{
    std::vector<Index> functions(static_cast<std::size_t>(n));
    std::iota(functions.begin(), functions.end(), 0);
    return functions;
}

Cones one(ConeKind kind, std::vector<Index> functions, std::vector<Real> parameters = {})
{
    const auto members = static_cast<Offset>(functions.size());
    const auto values = static_cast<Offset>(parameters.size());
    return Cones{
        .kinds = {kind},
        .starts = {0, members},
        .functions = std::move(functions),
        .parameter_starts = {0, values},
        .parameters = std::move(parameters),
    };
}

Cones pair()
{
    return Cones{
        .kinds = {ConeKind::Rotated, ConeKind::DualPower},
        .starts = {0, 3, 5},
        .functions = {4, 0, 2, 1, 3},
        .parameter_starts = {0, 0, 2},
        .parameters = {0.5, 2.0},
    };
}

void add(Model &model, const Cones &cones)
{
    model.add_cones(cones.kinds,
                    cones.starts,
                    cones.functions,
                    cones.parameter_starts,
                    cones.parameters);
}

std::string add_refusal(Model &model, const Cones &cones)
{
    return refusal([&] { add(model, cones); });
}

Model with_cones(std::size_t n)
{
    Model model = with_functions(1);
    for (std::size_t k = 0; k < n; ++k)
    {
        add(model, one(ConeKind::Nonnegative, {0}));
    }
    return model;
}

std::vector<Index> members_of(const Model &model, Index start, Index stop)
{
    const std::span<const Index> found = model.cone_functions(start, stop);
    return {found.begin(), found.end()};
}

std::vector<Bits> parameters_of(const Model &model, Index start, Index stop)
{
    std::vector<Bits> found;
    for (const Real value : model.cone_parameters(start, stop))
    {
        found.push_back(std::bit_cast<Bits>(value));
    }
    return found;
}

std::vector<Bits> bits_of(const std::vector<Real> &values)
{
    std::vector<Bits> found;
    for (const Real value : values)
    {
        found.push_back(std::bit_cast<Bits>(value));
    }
    return found;
}

std::string outcome(const Shape &shape)
{
    Model model = with_functions(10);
    const std::vector<Real> parameters(shape.parameters, 1.0);
    try
    {
        add(model, one(shape.kind, first(shape.dimension), parameters));
    }
    catch (const Error &error)
    {
        return error.what();
    }
    return model.n_cones() == 1 ? "" : "not added";
}

}

TEST(ConeTest, Added)
{
    Model model = with_functions(5);
    add(model, pair());
    ASSERT_EQ(model.n_cones(), 2);
    const std::span<const ConeKind> kinds = model.cone_kinds(0, 2);
    EXPECT_EQ(std::vector<ConeKind>(kinds.begin(), kinds.end()), pair().kinds);
    EXPECT_EQ(members_of(model, 0, 1), (std::vector<Index>{4, 0, 2}));
    EXPECT_EQ(members_of(model, 1, 2), (std::vector<Index>{1, 3}));
    EXPECT_EQ(members_of(model, 0, 2), pair().functions);
    EXPECT_EQ(members_of(model, 2, 2), std::vector<Index>{});
    EXPECT_EQ(parameters_of(model, 0, 1), bits_of({}));
    EXPECT_EQ(parameters_of(model, 1, 2), bits_of({0.5, 2.0}));
    add(model, one(ConeKind::Power, {2, 3}, {1.0}));
    const std::vector<Offset> none{0};
    model.add_cones({}, none, {}, none, {});
    ASSERT_EQ(model.n_cones(), 3);
    EXPECT_EQ(members_of(model, 1, 3), (std::vector<Index>{1, 3, 2, 3}));
    EXPECT_EQ(parameters_of(model, 0, 3), bits_of({0.5, 2.0, 1.0}));
}

TEST(ConeTest, Dimensions)
{
    for (const Shape &shape : kShapes)
    {
        const std::string wanted =
            shape.fault.empty() ? "" : "invalid argument: cone 0: " + shape.fault;
        EXPECT_EQ(outcome(shape), wanted) << static_cast<int>(shape.kind);
    }
}

TEST(ConeTest, Parameters)
{
    Model model = with_functions(3);
    const std::string refused = "invalid argument: cone 0: ";
    EXPECT_EQ(add_refusal(model, one(ConeKind::SecondOrder, {0, 1}, {1.0})),
              refused + "takes no parameter, given 1");
    EXPECT_EQ(add_refusal(model, one(ConeKind::Semidefinite, {0, 1, 2}, {1.0, 1.0})),
              refused + "takes no parameter, given 2");
    EXPECT_EQ(add_refusal(model, one(ConeKind::Power, {0, 1})),
              refused + "takes at least one parameter");
    EXPECT_EQ(add_refusal(model, one(ConeKind::DualPower, {0, 1})),
              refused + "takes at least one parameter");
    const std::vector<std::pair<Real, std::string>> refusals{
        {kNan, "parameter 1 is NaN"},
        {kInfinity, "parameter 1 is infinity"},
        {-kInfinity, "parameter 1 is minus infinity"},
        {0.0, "parameter 1 is 0, not positive"},
        {-0.0, "parameter 1 is -0, not positive"},
        {-2.0, "parameter 1 is -2, not positive"},
    };
    for (const auto &[value, fault] : refusals)
    {
        EXPECT_EQ(add_refusal(model, one(ConeKind::Power, {0, 1, 2}, {1.0, value})),
                  refused + fault);
    }
    EXPECT_EQ(model.n_cones(), 0);
    const Real least = std::numeric_limits<Real>::denorm_min();
    const Real most = (std::numeric_limits<Real>::max)();
    add(model, one(ConeKind::DualPower, {0, 1, 2}, {least, most}));
    EXPECT_EQ(parameters_of(model, 0, 1), bits_of({least, most}));
}

TEST(ConeTest, SharedFunction)
{
    Model model = with_functions(3);
    const Constraint constraint{.function = 1, .lower = 0.0, .upper = 1.0};
    model.add_constraints(std::span(&constraint, 1));
    add(model, one(ConeKind::SecondOrder, {1, 0}));
    add(model, one(ConeKind::Nonnegative, {2, 1}));
    ASSERT_EQ(model.n_cones(), 2);
    EXPECT_EQ(members_of(model, 0, 2), (std::vector<Index>{1, 0, 2, 1}));
    EXPECT_EQ(model.constraint(0).function, 1);
    EXPECT_EQ(model.n_functions(), 3);
}

TEST(ConeTest, Copied)
{
    Model model = with_functions(5);
    add(model, pair());
    const Model copy = model;
    EXPECT_EQ(difference(model, copy), "");
    add(model, one(ConeKind::Semidefinite, {0, 1, 2}));
    EXPECT_EQ(difference(copy, model), "cone count 2 against 3");
    EXPECT_EQ(copy.n_cones(), 2);
}

TEST(ConeTest, UnknownFunction)
{
    Model model = with_functions(5);
    add(model, one(ConeKind::Nonnegative, {0}));
    EXPECT_EQ(add_refusal(model, one(ConeKind::SecondOrder, {0, 5})),
              "invalid argument: cone 1: function 5 is unknown");
    EXPECT_EQ(add_refusal(model, one(ConeKind::SecondOrder, {-1, 0})),
              "invalid argument: cone 1: function -1 is unknown");
    Cones two = pair();
    two.functions[4] = 7;
    EXPECT_EQ(add_refusal(model, two), "invalid argument: cone 2: function 7 is unknown");
    EXPECT_EQ(model.n_cones(), 1);
    EXPECT_EQ(members_of(model, 0, 1), std::vector<Index>{0});
}

TEST(ConeTest, RepeatedFunction)
{
    Model model = with_functions(3);
    EXPECT_EQ(add_refusal(model, one(ConeKind::Rotated, {1, 2, 1})),
              "invalid argument: cone 0: function 1 is repeated");
    EXPECT_EQ(model.n_cones(), 0);
}

TEST(ConeTest, UnknownKind)
{
    Model model = with_functions(3);
    EXPECT_EQ(add_refusal(model, one(ConeKind{0}, {0})),
              "invalid argument: cone 0: kind 0 is unknown");
    EXPECT_EQ(add_refusal(model, one(ConeKind{9}, {0})),
              "invalid argument: cone 0: kind 9 is unknown");
    EXPECT_EQ(model.n_cones(), 0);
}

TEST(ConeTest, Offsets)
{
    Model model = with_functions(5);
    Cones shifted = pair();
    shifted.starts = {1, 3, 5};
    EXPECT_EQ(add_refusal(model, shifted), "invalid argument: argument 'off': begins at 1, not 0");
    Cones falling = pair();
    falling.parameter_starts = {0, 3, 2};
    EXPECT_EQ(add_refusal(model, falling),
              "invalid argument: argument 'prmoff': decreases from 3 to 2 at entry 2");
    EXPECT_EQ(model.n_cones(), 0);
}

TEST(ConeTest, Ranges)
{
    Model model = with_functions(5);
    add(model, pair());
    EXPECT_EQ(refusal([&] { model.cone_kinds(1, 3); }), "invalid argument: cone 2: unknown");
    EXPECT_EQ(refusal([&] { model.cone_functions(3, 3); }), "invalid argument: cone 3: unknown");
    EXPECT_EQ(refusal([&] { model.cone_parameters(0, -1); }),
              "invalid argument: argument 'end': -1 is below 'beg' 0");
}

TEST(ConeTest, OwnTolerance)
{
    expect_own(with_cones(3), Family::Cone);
}

TEST(ConeTest, ToleranceRefusal)
{
    expect_refused(with_cones(2), Family::Cone);
}

TEST(ConeTest, DeletedTolerance)
{
    expect_deleted(with_cones(2), Family::Cone);
}

TEST(ConeTest, ToleranceCopy)
{
    expect_copied(with_cones(2), Family::Cone);
}
