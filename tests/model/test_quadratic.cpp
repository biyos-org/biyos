#include "core/error.hpp"
#include "core/index.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"
#include "refusal.hpp"
#include "sample.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::difference;
using biyos::Index;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Offset;
using biyos::QuadraticTerm;
using biyos::Variable;
using biyos::test::failure;
using biyos::test::Quadratic;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kLarge = 1.0e308;

Model with_funs(std::size_t vars, std::size_t funs)
{
    const Variable free{
        .kind = biyos::VariableKind::Continuous,
        .lower = -kInfinity,
        .upper = kInfinity,
    };
    Model mdl;
    mdl.add_vars(std::vector<Variable>(vars, free));
    const std::vector<double> constants(funs, 0.0);
    const std::vector<Offset> starts(funs + 1, 0);
    mdl.add_funs(constants, starts, {});
    return mdl;
}

void add(Model &mdl, const Quadratic &quad)
{
    mdl.add_quad_terms(quad.functions, quad.starts, quad.terms);
}

Quadratic single(Index fun, std::vector<QuadraticTerm> terms)
{
    const auto size = static_cast<Offset>(terms.size());
    return Quadratic{.functions = {fun}, .starts = {0, size}, .terms = std::move(terms)};
}

std::string add_refusal(Model &mdl, const Quadratic &quad)
{
    return refusal([&] { add(mdl, quad); });
}

std::uint64_t bits_of(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

void expect_pairs(std::span<const QuadraticTerm> found, const std::vector<QuadraticTerm> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        const double coefficient = found[k].coefficient;
        EXPECT_EQ(found[k].row, wanted[k].row) << "term " << k;
        EXPECT_EQ(found[k].column, wanted[k].column) << "term " << k;
        EXPECT_EQ(bits_of(coefficient), bits_of(wanted[k].coefficient)) << coefficient;
    }
}

std::vector<QuadraticTerm> held(const Model &mdl)
{
    const std::span<const QuadraticTerm> found = mdl.quad_terms(0, mdl.n_funs());
    return {found.begin(), found.end()};
}

template <typename Call> void expect_internal(Call call)
{
    failure(call, biyos::ErrorKind::Internal);
}

}

TEST(QuadraticTest, Sorted)
{
    Model mdl = with_funs(4, 2);
    const Quadratic quad{
        .functions = {1, 0},
        .starts = {0, 3, 5},
        .terms = {{3, 1, 1.5}, {0, 2, -2.0}, {1, 1, 0.5}, {2, 3, 4.0}, {0, 0, 1.0}},
    };
    add(mdl, quad);
    expect_pairs(mdl.quad_terms(0, 1), {{0, 0, 1.0}, {2, 3, 4.0}});
    expect_pairs(mdl.quad_terms(1, 1), {{0, 2, -2.0}, {1, 1, 0.5}, {1, 3, 1.5}});
}

TEST(QuadraticTest, Swapped)
{
    Model mdl = with_funs(3, 1);
    add(mdl, single(0, {{2, 0, 3.0}, {1, 1, -1.0}, {2, 1, 0.5}}));
    expect_pairs(mdl.quad_terms(0, 1), {{0, 2, 3.0}, {1, 1, -1.0}, {1, 2, 0.5}});
}

TEST(QuadraticTest, UnknownFunction)
{
    Model mdl = with_funs(2, 2);
    EXPECT_EQ(add_refusal(mdl, single(2, {})), "invalid argument: function 2: unknown");
    EXPECT_EQ(add_refusal(mdl, single(-1, {{0, 1, 1.0}})),
              "invalid argument: function -1: unknown");
    EXPECT_TRUE(held(mdl).empty());
}

TEST(QuadraticTest, RepeatedFunction)
{
    Model mdl = with_funs(2, 3);
    const Quadratic twice{
        .functions = {1, 0, 1},
        .starts = {0, 1, 1, 2},
        .terms = {{0, 1, 1.0}, {1, 1, 2.0}},
    };
    EXPECT_EQ(add_refusal(mdl, twice),
              "invalid argument: argument 'funidx': function 1 is repeated");
    EXPECT_TRUE(held(mdl).empty());
}

TEST(QuadraticTest, UnknownVariable)
{
    Model mdl = with_funs(3, 1);
    const std::string refused = "invalid argument: function 0: variable ";
    EXPECT_EQ(add_refusal(mdl, single(0, {{0, 1, 1.0}, {3, 0, 1.0}})), refused + "3 is unknown");
    EXPECT_EQ(add_refusal(mdl, single(0, {{1, 3, 1.0}})), refused + "3 is unknown");
    EXPECT_EQ(add_refusal(mdl, single(0, {{-1, 0, 1.0}})), refused + "-1 is unknown");
    EXPECT_EQ(add_refusal(mdl, single(0, {{0, -2, 1.0}})), refused + "-2 is unknown");
    EXPECT_TRUE(held(mdl).empty());
}

TEST(QuadraticTest, RepeatedPair)
{
    Model mdl = with_funs(3, 2);
    EXPECT_EQ(add_refusal(mdl, single(1, {{0, 2, 1.0}, {1, 1, 1.0}, {0, 2, 2.0}})),
              "invalid argument: function 1: pair (0, 2) is repeated");
    EXPECT_EQ(add_refusal(mdl, single(1, {{2, 0, 1.0}, {0, 2, 2.0}})),
              "invalid argument: function 1: pair (0, 2) is repeated");
    EXPECT_TRUE(held(mdl).empty());
    const Quadratic apart{
        .functions = {0, 1},
        .starts = {0, 1, 2},
        .terms = {{0, 2, 1.0}, {0, 2, 2.0}},
    };
    add(mdl, apart);
    expect_pairs(held(mdl), {{0, 2, 1.0}, {0, 2, 2.0}});
}

TEST(QuadraticTest, Coefficients)
{
    Model mdl = with_funs(2, 1);
    const std::string refused = "invalid argument: function 0: coefficient of pair (1, 0) ";
    EXPECT_EQ(add_refusal(mdl, single(0, {{0, 0, 1.0}, {1, 0, kNan}})), refused + "is NaN");
    EXPECT_EQ(add_refusal(mdl, single(0, {{1, 0, kInfinity}})), refused + "is infinity");
    EXPECT_EQ(add_refusal(mdl, single(0, {{1, 0, -kInfinity}})), refused + "is minus infinity");
    EXPECT_TRUE(held(mdl).empty());
}

TEST(QuadraticTest, Sums)
{
    Model mdl = with_funs(2, 1);
    add(mdl, single(0, {{0, 1, kLarge}, {1, 1, -kLarge}}));
    const std::string refused = "invalid argument: function 0: sum of coefficients of pair ";
    EXPECT_EQ(add_refusal(mdl, single(0, {{1, 0, kLarge}})), refused + "(0, 1) is infinity");
    EXPECT_EQ(add_refusal(mdl, single(0, {{0, 0, 1.0}, {1, 1, -kLarge}})),
              refused + "(1, 1) is minus infinity");
    expect_pairs(held(mdl), {{0, 1, kLarge}, {1, 1, -kLarge}});
}

TEST(QuadraticTest, Zeros)
{
    Model mdl = with_funs(3, 1);
    add(mdl, single(0, {{2, 2, 0.0}, {0, 1, 1.0}, {1, 1, -0.0}}));
    add(mdl, single(0, {{1, 0, -1.0}}));
    expect_pairs(held(mdl), {{0, 1, 0.0}, {1, 1, -0.0}, {2, 2, 0.0}});
}

TEST(QuadraticTest, TwoCalls)
{
    Model mdl = with_funs(3, 1);
    add(mdl, single(0, {{0, 1, 1.0}, {1, 1, 2.0}}));
    add(mdl, single(0, {{1, 2, 4.0}, {1, 0, 0.5}, {0, 0, 3.0}}));
    expect_pairs(held(mdl), {{0, 0, 3.0}, {0, 1, 1.5}, {1, 1, 2.0}, {1, 2, 4.0}});
}

TEST(QuadraticTest, Merged)
{
    Model mdl = with_funs(4, 5);
    const Quadratic first{
        .functions = {0, 1, 3, 4},
        .starts = {0, 1, 2, 4, 5},
        .terms = {{0, 0, 1.0}, {1, 1, 2.0}, {0, 1, 3.0}, {2, 2, 4.0}, {1, 2, 5.0}},
    };
    add(mdl, first);
    const Quadratic second{
        .functions = {3, 0, 4, 2},
        .starts = {0, 3, 4, 6, 7},
        .terms = {{3, 3, 1.0},
                  {0, 0, 1.0},
                  {1, 0, 1.0},
                  {0, 1, 1.0},
                  {3, 0, 1.0},
                  {2, 3, 1.0},
                  {2, 1, 1.0}},
    };
    add(mdl, second);
    expect_pairs(mdl.quad_terms(0, 1), {{0, 0, 1.0}, {0, 1, 1.0}});
    expect_pairs(mdl.quad_terms(1, 1), {{1, 1, 2.0}});
    expect_pairs(mdl.quad_terms(2, 1), {{1, 2, 1.0}});
    expect_pairs(mdl.quad_terms(3, 1), {{0, 0, 1.0}, {0, 1, 4.0}, {2, 2, 4.0}, {3, 3, 1.0}});
    expect_pairs(mdl.quad_terms(4, 1), {{0, 3, 1.0}, {1, 2, 5.0}, {2, 3, 1.0}});
    EXPECT_EQ(held(mdl).size(), 11U);
}

TEST(QuadraticTest, Shifted)
{
    Model mdl = with_funs(4, 3);
    const Quadratic first{
        .functions = {0, 1, 2},
        .starts = {0, 1, 3, 4},
        .terms = {{1, 1, 2.0}, {0, 0, 1.0}, {0, 1, 4.0}, {2, 3, 1.0}},
    };
    add(mdl, first);
    const Quadratic second{
        .functions = {1, 0},
        .starts = {0, 1, 2},
        .terms = {{3, 2, 3.0}, {2, 2, 1.0}},
    };
    add(mdl, second);
    expect_pairs(mdl.quad_terms(0, 1), {{1, 1, 2.0}, {2, 2, 1.0}});
    expect_pairs(mdl.quad_terms(1, 1), {{0, 0, 1.0}, {0, 1, 4.0}, {2, 3, 3.0}});
    expect_pairs(mdl.quad_terms(2, 1), {{2, 3, 1.0}});
}

TEST(QuadraticTest, Unchanged)
{
    Model mdl = with_funs(2, 3);
    add(mdl, single(1, {{0, 1, kLarge}}));
    const Quadratic unknown{
        .functions = {0, 2},
        .starts = {0, 1, 2},
        .terms = {{0, 0, 1.0}, {0, 2, 1.0}},
    };
    EXPECT_NE(add_refusal(mdl, unknown), "");
    const Quadratic infinite{
        .functions = {2, 1},
        .starts = {0, 1, 2},
        .terms = {{1, 1, 1.0}, {1, 0, kInfinity}},
    };
    EXPECT_NE(add_refusal(mdl, infinite), "");
    const Quadratic overflow{
        .functions = {0, 1},
        .starts = {0, 1, 2},
        .terms = {{1, 1, 1.0}, {1, 0, kLarge}},
    };
    EXPECT_NE(add_refusal(mdl, overflow), "");
    expect_pairs(mdl.quad_terms(0, 1), {});
    expect_pairs(mdl.quad_terms(1, 1), {{0, 1, kLarge}});
    expect_pairs(mdl.quad_terms(2, 1), {});
}

TEST(QuadraticTest, Atomic)
{
    Model mdl = with_funs(2, 3);
    add(mdl, single(0, {{0, 0, kLarge}}));
    const Quadratic overflow{
        .functions = {2, 0},
        .starts = {0, 1, 2},
        .terms = {{1, 1, 1.0}, {0, 0, kLarge}},
    };
    EXPECT_EQ(add_refusal(mdl, overflow),
              "invalid argument: function 0: sum of coefficients of pair (0, 0) is infinity");
    mdl.add_funs(std::vector<double>{0.0}, std::vector<Offset>{0, 0}, {});
    expect_pairs(held(mdl), {{0, 0, kLarge}});
    EXPECT_TRUE(mdl.quad_terms(2, 2).empty());
}

TEST(QuadraticTest, Order)
{
    Model mdl = with_funs(3, 3);
    const Quadratic large{
        .functions = {1, 2},
        .starts = {0, 1, 2},
        .terms = {{0, 0, kLarge}, {1, 1, kLarge}},
    };
    add(mdl, large);
    const Quadratic listed{.functions = {1, 1, 5}, .starts = {0, 0, 0, 0}, .terms = {}};
    EXPECT_EQ(add_refusal(mdl, listed), "invalid argument: function 5: unknown");
    const Quadratic sorted{
        .functions = {1, 0},
        .starts = {0, 2, 3},
        .terms = {{0, 1, 1.0}, {1, 0, 1.0}, {0, 3, 1.0}},
    };
    EXPECT_EQ(add_refusal(mdl, sorted), "invalid argument: function 1: pair (0, 1) is repeated");
    const Quadratic summed{
        .functions = {1, 0},
        .starts = {0, 1, 2},
        .terms = {{0, 0, kLarge}, {0, 3, 1.0}},
    };
    EXPECT_EQ(add_refusal(mdl, summed), "invalid argument: function 0: variable 3 is unknown");
    const std::string sum = "invalid argument: function ";
    const Quadratic falling{
        .functions = {2, 1},
        .starts = {0, 1, 2},
        .terms = {{1, 1, kLarge}, {0, 0, kLarge}},
    };
    const std::string second = "2: sum of coefficients of pair (1, 1) is infinity";
    EXPECT_EQ(add_refusal(mdl, falling), sum + second);
    const Quadratic rising{
        .functions = {1, 2},
        .starts = {0, 1, 2},
        .terms = {{0, 0, kLarge}, {1, 1, kLarge}},
    };
    const std::string first = "1: sum of coefficients of pair (0, 0) is infinity";
    EXPECT_EQ(add_refusal(mdl, rising), sum + first);
    expect_pairs(held(mdl), {{0, 0, kLarge}, {1, 1, kLarge}});
}

TEST(QuadraticTest, Later)
{
    Model mdl = with_funs(2, 2);
    add(mdl, single(0, {{0, 1, 1.0}}));
    const std::vector<double> constants{2.0, 3.0};
    const std::vector<Offset> starts{0, 1, 1};
    mdl.add_funs(constants, starts, {LinearTerm{.variable = 1, .coefficient = 5.0}});
    expect_pairs(mdl.quad_terms(0, 4), {{0, 1, 1.0}});
    expect_pairs(mdl.quad_terms(2, 2), {});
    add(mdl, single(2, {{1, 1, 2.0}}));
    expect_pairs(mdl.quad_terms(0, 1), {{0, 1, 1.0}});
    expect_pairs(mdl.quad_terms(2, 1), {{1, 1, 2.0}});
    EXPECT_EQ(mdl.terms(2, 1).size(), 1U);
    EXPECT_TRUE(mdl.quad_terms(3, 1).empty());
}

TEST(QuadraticTest, ReadRange)
{
    Model mdl = with_funs(2, 3);
    const Quadratic quad{
        .functions = {0, 2},
        .starts = {0, 1, 3},
        .terms = {{0, 0, 1.0}, {0, 1, 2.0}, {1, 1, 3.0}},
    };
    add(mdl, quad);
    expect_pairs(mdl.quad_terms(1, 2), {{0, 1, 2.0}, {1, 1, 3.0}});
    EXPECT_TRUE(mdl.quad_terms(1, 1).empty());
    EXPECT_TRUE(mdl.quad_terms(3, 0).empty());
    EXPECT_EQ(refusal([&] { mdl.quad_terms(-1, 1); }), "invalid argument: function -1: unknown");
    EXPECT_EQ(refusal([&] { mdl.quad_terms(1, 3); }), "invalid argument: function 3: unknown");
    EXPECT_EQ(refusal([&] { mdl.quad_terms(0, -1); }),
              "invalid argument: argument 'n': -1 is negative");
}

TEST(QuadraticTest, BadStarts)
{
    Model mdl = with_funs(2, 2);
    const Quadratic late{.functions = {0}, .starts = {1, 1}, .terms = {{0, 0, 1.0}}};
    EXPECT_EQ(add_refusal(mdl, late), "invalid argument: argument 'off': begins at 1, not 0");
    const Quadratic falling{.functions = {0, 1}, .starts = {0, 2, 1}, .terms = {{0, 0, 1.0}}};
    EXPECT_EQ(add_refusal(mdl, falling),
              "invalid argument: argument 'off': decreases from 2 to 1 at entry 2");
    EXPECT_TRUE(held(mdl).empty());
}

TEST(QuadraticTest, Mismatch)
{
    Model mdl = with_funs(1, 1);
    const Quadratic short_starts{.functions = {0}, .starts = {0}, .terms = {}};
    expect_internal([&] { add(mdl, short_starts); });
    const Quadratic long_terms{.functions = {0}, .starts = {0, 2}, .terms = {{0, 0, 1.0}}};
    expect_internal([&] { add(mdl, long_terms); });
    EXPECT_TRUE(held(mdl).empty());
}

TEST(QuadraticTest, Room)
{
    Model mdl = with_funs(1, 1);
    const Offset most = (std::numeric_limits<Offset>::max)();
    const auto call = [&] { mdl.check_quad_room(most); };
    EXPECT_EQ(failure(call, biyos::ErrorKind::NoMemory),
              "out of memory: model: no quadratic term offset left");
    const Offset large = Offset{1} << 59;
    const auto beyond = [&] { mdl.check_quad_room(large); };
    EXPECT_EQ(failure(beyond, biyos::ErrorKind::NoMemory),
              "out of memory: model: no quadratic term offset left");
    mdl.check_quad_room(1);
    add(mdl, single(0, {{0, 0, 1.0}}));
    expect_pairs(held(mdl), {{0, 0, 1.0}});
}

TEST(QuadraticTest, Moved)
{
    Model mdl = with_funs(1, 1);
    add(mdl, single(0, {{0, 0, 2.0}}));
    const Model taken = std::move(mdl);
    expect_pairs(held(taken), {{0, 0, 2.0}});
    EXPECT_TRUE(mdl.quad_terms(0, 0).empty());
    mdl.add_vars(std::vector<Variable>(1, taken.variable(0)));
    mdl.add_funs(std::vector<double>{0.0}, std::vector<Offset>{0, 0}, {});
    add(mdl, single(0, {{0, 0, 3.0}}));
    expect_pairs(held(mdl), {{0, 0, 3.0}});
}

TEST(QuadraticTest, Copied)
{
    Model mdl = with_funs(3, 2);
    add(mdl, single(1, {{2, 0, 1.5}, {1, 1, -0.0}}));
    Model copy = mdl;
    EXPECT_EQ(difference(mdl, copy), "");
    add(copy, single(1, {{0, 2, 1.0}}));
    EXPECT_EQ(difference(mdl, copy), "function 1: quadratic term 0: coefficient 1.5 against 2.5");
    expect_pairs(held(mdl), {{0, 2, 1.5}, {1, 1, -0.0}});
}
