#include "core/error.hpp"
#include "core/index.hpp"
#include "model/model.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::LinearTerm;
using biyos::Model;
using biyos::Offset;
using biyos::Variable;
using biyos::test::failure;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

struct Functions
{
    std::vector<double> constants;
    std::vector<Offset> starts;
    std::vector<LinearTerm> terms;
};

Model with_vars(std::size_t n)
{
    const Variable free{
        .kind = biyos::VariableKind::Continuous,
        .lower = -kInfinity,
        .upper = kInfinity,
    };
    Model mdl;
    mdl.add_vars(std::vector<Variable>(n, free));
    return mdl;
}

void add(Model &mdl, const Functions &funs)
{
    mdl.add_funs(funs.constants, funs.starts, funs.terms);
}

Functions single(double constant, std::vector<LinearTerm> terms)
{
    const auto size = static_cast<Offset>(terms.size());
    return Functions{.constants = {constant}, .starts = {0, size}, .terms = std::move(terms)};
}

std::string add_refusal(Model &mdl, const Functions &funs)
{
    return refusal([&] { add(mdl, funs); });
}

void expect_terms(std::span<const LinearTerm> found, const std::vector<LinearTerm> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        EXPECT_EQ(found[k].variable, wanted[k].variable) << "term " << k;
        EXPECT_DOUBLE_EQ(found[k].coefficient, wanted[k].coefficient) << "term " << k;
    }
}

void expect_constants(std::span<const double> found, const std::vector<double> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        EXPECT_DOUBLE_EQ(found[k], wanted[k]) << "constant " << k;
    }
}

template <typename Call> void expect_internal(Call call)
{
    failure(call, biyos::ErrorKind::Internal);
}

}

TEST(FunctionTest, Sorted)
{
    Model mdl = with_vars(4);
    const Functions funs{
        .constants = {0.0, 0.0},
        .starts = {0, 3, 5},
        .terms = {{3, 1.5}, {0, -2.0}, {2, 0.5}, {1, 4.0}, {0, 1.0}},
    };
    add(mdl, funs);
    ASSERT_EQ(mdl.n_funs(), 2);
    expect_terms(mdl.terms(0, 1), {{0, -2.0}, {2, 0.5}, {3, 1.5}});
    expect_terms(mdl.terms(1, 1), {{0, 1.0}, {1, 4.0}});
}

TEST(FunctionTest, Constants)
{
    Model mdl = with_vars(1);
    const Functions funs{
        .constants = {2.5, 0.0, -1.0e300},
        .starts = {0, 1, 1, 1},
        .terms = {{0, 1.0}},
    };
    add(mdl, funs);
    expect_constants(mdl.constants(0, 3), {2.5, 0.0, -1.0e300});
    expect_terms(mdl.terms(0, 1), {{0, 1.0}});
}

TEST(FunctionTest, BadConstants)
{
    Model mdl = with_vars(1);
    const std::string refused = "invalid argument: function 0: constant ";
    EXPECT_EQ(add_refusal(mdl, single(kNan, {})), refused + "is NaN");
    EXPECT_EQ(add_refusal(mdl, single(kInfinity, {{0, 1.0}})), refused + "is infinity");
    EXPECT_EQ(add_refusal(mdl, single(-kInfinity, {})), refused + "is minus infinity");
    EXPECT_EQ(mdl.n_funs(), 0);
}

TEST(FunctionTest, UnknownVariable)
{
    Model mdl = with_vars(3);
    add(mdl, single(0.0, {{2, 1.0}}));
    EXPECT_EQ(add_refusal(mdl, single(0.0, {{0, 1.0}, {3, 1.0}})),
              "invalid argument: function 1: variable 3 is unknown");
    EXPECT_EQ(add_refusal(mdl, single(0.0, {{-1, 1.0}})),
              "invalid argument: function 1: variable -1 is unknown");
    EXPECT_EQ(mdl.n_funs(), 1);
}

TEST(FunctionTest, Repeated)
{
    Model mdl = with_vars(3);
    EXPECT_EQ(add_refusal(mdl, single(0.0, {{2, 1.0}, {0, 1.0}, {2, 3.0}})),
              "invalid argument: function 0: variable 2 is repeated");
    EXPECT_EQ(mdl.n_funs(), 0);
}

TEST(FunctionTest, Coefficients)
{
    Model mdl = with_vars(2);
    const std::string refused = "invalid argument: function 0: coefficient of variable 1 ";
    EXPECT_EQ(add_refusal(mdl, single(0.0, {{0, 1.0}, {1, kNan}})), refused + "is NaN");
    EXPECT_EQ(add_refusal(mdl, single(0.0, {{1, kInfinity}})), refused + "is infinity");
    EXPECT_EQ(add_refusal(mdl, single(0.0, {{1, -kInfinity}})), refused + "is minus infinity");
    EXPECT_EQ(mdl.n_funs(), 0);
}

TEST(FunctionTest, Zeros)
{
    Model mdl = with_vars(3);
    add(mdl, single(0.0, {{2, 0.0}, {0, 1.0}, {1, -0.0}}));
    expect_terms(mdl.terms(0, 1), {{0, 1.0}, {1, 0.0}, {2, 0.0}});
}

TEST(FunctionTest, BadStarts)
{
    Model mdl = with_vars(2);
    const Functions late{.constants = {0.0}, .starts = {1, 1}, .terms = {{0, 1.0}}};
    EXPECT_EQ(add_refusal(mdl, late), "invalid argument: argument 'off': begins at 1, not 0");
    const Functions falling{.constants = {0.0, 0.0}, .starts = {0, 2, 1}, .terms = {{0, 1.0}}};
    EXPECT_EQ(add_refusal(mdl, falling),
              "invalid argument: argument 'off': decreases from 2 to 1 at entry 2");
    EXPECT_EQ(mdl.n_funs(), 0);
}

TEST(FunctionTest, Appended)
{
    Model mdl = with_vars(3);
    add(mdl, single(1.0, {{1, 1.0}, {0, 2.0}}));
    const Functions more{
        .constants = {2.0, 3.0},
        .starts = {0, 0, 2},
        .terms = {{2, 3.0}, {1, 4.0}},
    };
    add(mdl, more);
    ASSERT_EQ(mdl.n_funs(), 3);
    expect_constants(mdl.constants(0, 3), {1.0, 2.0, 3.0});
    expect_terms(mdl.terms(0, 3), {{0, 2.0}, {1, 1.0}, {1, 4.0}, {2, 3.0}});
    expect_terms(mdl.terms(1, 1), {});
    expect_terms(mdl.terms(2, 1), {{1, 4.0}, {2, 3.0}});
}

TEST(FunctionTest, Unchanged)
{
    Model mdl = with_vars(2);
    add(mdl, single(1.0, {{1, 1.0}}));
    const Functions repeated{
        .constants = {2.0, 3.0},
        .starts = {0, 1, 3},
        .terms = {{0, 1.0}, {1, 2.0}, {1, 3.0}},
    };
    EXPECT_NE(add_refusal(mdl, repeated), "");
    const Functions infinite{
        .constants = {2.0, kInfinity},
        .starts = {0, 1, 1},
        .terms = {{0, 1.0}},
    };
    EXPECT_NE(add_refusal(mdl, infinite), "");
    ASSERT_EQ(mdl.n_funs(), 1);
    expect_constants(mdl.constants(0, 1), {1.0});
    expect_terms(mdl.terms(0, 1), {{1, 1.0}});
}

TEST(FunctionTest, ReadRange)
{
    Model mdl = with_vars(2);
    const Functions funs{
        .constants = {1.0, 2.0, 3.0},
        .starts = {0, 1, 3, 4},
        .terms = {{0, 1.0}, {1, 2.0}, {0, 3.0}, {1, 4.0}},
    };
    add(mdl, funs);
    expect_constants(mdl.constants(1, 2), {2.0, 3.0});
    expect_terms(mdl.terms(1, 2), {{0, 3.0}, {1, 2.0}, {1, 4.0}});
    EXPECT_TRUE(mdl.constants(3, 0).empty());
    EXPECT_TRUE(mdl.terms(3, 0).empty());
    EXPECT_EQ(refusal([&] { mdl.constants(-1, 1); }), "invalid argument: function -1: unknown");
    EXPECT_EQ(refusal([&] { mdl.terms(1, 3); }), "invalid argument: function 3: unknown");
    EXPECT_EQ(refusal([&] { mdl.terms(4, 0); }), "invalid argument: function 4: unknown");
    EXPECT_EQ(refusal([&] { mdl.terms(0, -1); }), "invalid argument: argument 'n': -1 is negative");
}

TEST(FunctionTest, Mismatch)
{
    Model mdl = with_vars(1);
    const Functions short_starts{.constants = {0.0}, .starts = {0}, .terms = {}};
    expect_internal([&] { add(mdl, short_starts); });
    const Functions long_terms{.constants = {0.0}, .starts = {0, 2}, .terms = {}};
    expect_internal([&] { add(mdl, long_terms); });
    EXPECT_EQ(mdl.n_funs(), 0);
}

TEST(FunctionTest, Room)
{
    Model mdl = with_vars(1);
    const Offset most = (std::numeric_limits<Offset>::max)();
    const auto call = [&] { mdl.check_room(1, most); };
    EXPECT_EQ(failure(call, biyos::ErrorKind::NoMemory),
              "out of memory: model: no term offset left");
    const Offset large = Offset{1} << 59;
    const auto beyond = [&] { mdl.check_room(1, large); };
    EXPECT_EQ(failure(beyond, biyos::ErrorKind::NoMemory),
              "out of memory: model: no term offset left");
    mdl.check_room(1, 1);
    add(mdl, single(0.0, {{0, 1.0}}));
    EXPECT_EQ(mdl.n_funs(), 1);
}

TEST(FunctionTest, Moved)
{
    Model mdl = with_vars(1);
    add(mdl, single(1.0, {{0, 2.0}}));
    const Model taken = std::move(mdl);
    expect_terms(taken.terms(0, 1), {{0, 2.0}});
    EXPECT_EQ(mdl.n_funs(), 0);
    EXPECT_TRUE(mdl.terms(0, 0).empty());
    add(mdl, single(0.0, {}));
    expect_terms(mdl.terms(0, 1), {});
    Model assigned;
    assigned = std::move(mdl);
    add(mdl, single(0.0, {}));
    EXPECT_EQ(mdl.n_funs(), 1);
    EXPECT_EQ(assigned.n_funs(), 1);
}
