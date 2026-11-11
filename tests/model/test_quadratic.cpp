#include "core/error.hpp"
#include "core/index.hpp"
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

Model with_functions(std::size_t variables, std::size_t functions)
{
    const Variable free{
        .kind = biyos::VariableKind::Continuous,
        .lower = -kInfinity,
        .upper = kInfinity,
    };
    Model model;
    model.add_variables(std::vector<Variable>(variables, free));
    const std::vector<double> constants(functions, 0.0);
    const std::vector<Offset> starts(functions + 1, 0);
    model.add_functions(constants, starts, {});
    return model;
}

void add(Model &model, const Quadratic &quadratic)
{
    model.add_quadratic_terms(quadratic.functions, quadratic.starts, quadratic.terms);
}

Quadratic single(Index function, std::vector<QuadraticTerm> terms)
{
    const auto size = static_cast<Offset>(terms.size());
    return Quadratic{.functions = {function}, .starts = {0, size}, .terms = std::move(terms)};
}

std::string add_refusal(Model &model, const Quadratic &quadratic)
{
    return refusal([&] { add(model, quadratic); });
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
        EXPECT_EQ(found[k].first, wanted[k].first) << "term " << k;
        EXPECT_EQ(found[k].second, wanted[k].second) << "term " << k;
        EXPECT_EQ(bits_of(coefficient), bits_of(wanted[k].coefficient)) << coefficient;
    }
}

std::vector<QuadraticTerm> held(const Model &model)
{
    const std::span<const QuadraticTerm> found = model.quadratic_terms(0, model.function_count());
    return {found.begin(), found.end()};
}

template <typename Call> void expect_internal(Call call)
{
    failure(call, biyos::ErrorKind::Internal);
}

}

TEST(QuadraticTest, Sorted)
{
    Model model = with_functions(4, 2);
    const Quadratic quadratic{
        .functions = {1, 0},
        .starts = {0, 3, 5},
        .terms = {{3, 1, 1.5}, {0, 2, -2.0}, {1, 1, 0.5}, {2, 3, 4.0}, {0, 0, 1.0}},
    };
    add(model, quadratic);
    expect_pairs(model.quadratic_terms(0, 1), {{0, 0, 1.0}, {2, 3, 4.0}});
    expect_pairs(model.quadratic_terms(1, 1), {{0, 2, -2.0}, {1, 1, 0.5}, {1, 3, 1.5}});
}

TEST(QuadraticTest, Swapped)
{
    Model model = with_functions(3, 1);
    add(model, single(0, {{2, 0, 3.0}, {1, 1, -1.0}, {2, 1, 0.5}}));
    expect_pairs(model.quadratic_terms(0, 1), {{0, 2, 3.0}, {1, 1, -1.0}, {1, 2, 0.5}});
}

TEST(QuadraticTest, UnknownFunction)
{
    Model model = with_functions(2, 2);
    EXPECT_EQ(add_refusal(model, single(2, {})), "invalid argument: function 2: unknown");
    EXPECT_EQ(add_refusal(model, single(-1, {{0, 1, 1.0}})),
              "invalid argument: function -1: unknown");
    EXPECT_TRUE(held(model).empty());
}

TEST(QuadraticTest, RepeatedFunction)
{
    Model model = with_functions(2, 3);
    const Quadratic twice{
        .functions = {1, 0, 1},
        .starts = {0, 1, 1, 2},
        .terms = {{0, 1, 1.0}, {1, 1, 2.0}},
    };
    EXPECT_EQ(add_refusal(model, twice),
              "invalid argument: argument 'functions': function 1 is repeated");
    EXPECT_TRUE(held(model).empty());
}

TEST(QuadraticTest, UnknownVariable)
{
    Model model = with_functions(3, 1);
    const std::string refused = "invalid argument: function 0: variable ";
    EXPECT_EQ(add_refusal(model, single(0, {{0, 1, 1.0}, {3, 0, 1.0}})), refused + "3 is unknown");
    EXPECT_EQ(add_refusal(model, single(0, {{1, 3, 1.0}})), refused + "3 is unknown");
    EXPECT_EQ(add_refusal(model, single(0, {{-1, 0, 1.0}})), refused + "-1 is unknown");
    EXPECT_EQ(add_refusal(model, single(0, {{0, -2, 1.0}})), refused + "-2 is unknown");
    EXPECT_TRUE(held(model).empty());
}

TEST(QuadraticTest, RepeatedPair)
{
    Model model = with_functions(3, 2);
    EXPECT_EQ(add_refusal(model, single(1, {{0, 2, 1.0}, {1, 1, 1.0}, {0, 2, 2.0}})),
              "invalid argument: function 1: pair (0, 2) is repeated");
    EXPECT_EQ(add_refusal(model, single(1, {{2, 0, 1.0}, {0, 2, 2.0}})),
              "invalid argument: function 1: pair (0, 2) is repeated");
    EXPECT_TRUE(held(model).empty());
    const Quadratic apart{
        .functions = {0, 1},
        .starts = {0, 1, 2},
        .terms = {{0, 2, 1.0}, {0, 2, 2.0}},
    };
    add(model, apart);
    expect_pairs(held(model), {{0, 2, 1.0}, {0, 2, 2.0}});
}

TEST(QuadraticTest, Coefficients)
{
    Model model = with_functions(2, 1);
    const std::string refused = "invalid argument: function 0: coefficient of pair (1, 0) ";
    EXPECT_EQ(add_refusal(model, single(0, {{0, 0, 1.0}, {1, 0, kNan}})), refused + "is NaN");
    EXPECT_EQ(add_refusal(model, single(0, {{1, 0, kInfinity}})), refused + "is infinity");
    EXPECT_EQ(add_refusal(model, single(0, {{1, 0, -kInfinity}})), refused + "is minus infinity");
    EXPECT_TRUE(held(model).empty());
}

TEST(QuadraticTest, Sums)
{
    Model model = with_functions(2, 1);
    add(model, single(0, {{0, 1, kLarge}, {1, 1, -kLarge}}));
    const std::string refused = "invalid argument: function 0: sum of coefficients of pair ";
    EXPECT_EQ(add_refusal(model, single(0, {{1, 0, kLarge}})), refused + "(0, 1) is infinity");
    EXPECT_EQ(add_refusal(model, single(0, {{0, 0, 1.0}, {1, 1, -kLarge}})),
              refused + "(1, 1) is minus infinity");
    expect_pairs(held(model), {{0, 1, kLarge}, {1, 1, -kLarge}});
}

TEST(QuadraticTest, Zeros)
{
    Model model = with_functions(3, 1);
    add(model, single(0, {{2, 2, 0.0}, {0, 1, 1.0}, {1, 1, -0.0}}));
    add(model, single(0, {{1, 0, -1.0}}));
    expect_pairs(held(model), {{0, 1, 0.0}, {1, 1, -0.0}, {2, 2, 0.0}});
}

TEST(QuadraticTest, TwoCalls)
{
    Model model = with_functions(3, 1);
    add(model, single(0, {{0, 1, 1.0}, {1, 1, 2.0}}));
    add(model, single(0, {{1, 2, 4.0}, {1, 0, 0.5}, {0, 0, 3.0}}));
    expect_pairs(held(model), {{0, 0, 3.0}, {0, 1, 1.5}, {1, 1, 2.0}, {1, 2, 4.0}});
}

TEST(QuadraticTest, Merged)
{
    Model model = with_functions(4, 5);
    const Quadratic first{
        .functions = {0, 1, 3, 4},
        .starts = {0, 1, 2, 4, 5},
        .terms = {{0, 0, 1.0}, {1, 1, 2.0}, {0, 1, 3.0}, {2, 2, 4.0}, {1, 2, 5.0}},
    };
    add(model, first);
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
    add(model, second);
    expect_pairs(model.quadratic_terms(0, 1), {{0, 0, 1.0}, {0, 1, 1.0}});
    expect_pairs(model.quadratic_terms(1, 1), {{1, 1, 2.0}});
    expect_pairs(model.quadratic_terms(2, 1), {{1, 2, 1.0}});
    expect_pairs(model.quadratic_terms(3, 1), {{0, 0, 1.0}, {0, 1, 4.0}, {2, 2, 4.0}, {3, 3, 1.0}});
    expect_pairs(model.quadratic_terms(4, 1), {{0, 3, 1.0}, {1, 2, 5.0}, {2, 3, 1.0}});
    EXPECT_EQ(held(model).size(), 11U);
}

TEST(QuadraticTest, Shifted)
{
    Model model = with_functions(4, 3);
    const Quadratic first{
        .functions = {0, 1, 2},
        .starts = {0, 1, 3, 4},
        .terms = {{1, 1, 2.0}, {0, 0, 1.0}, {0, 1, 4.0}, {2, 3, 1.0}},
    };
    add(model, first);
    const Quadratic second{
        .functions = {1, 0},
        .starts = {0, 1, 2},
        .terms = {{3, 2, 3.0}, {2, 2, 1.0}},
    };
    add(model, second);
    expect_pairs(model.quadratic_terms(0, 1), {{1, 1, 2.0}, {2, 2, 1.0}});
    expect_pairs(model.quadratic_terms(1, 1), {{0, 0, 1.0}, {0, 1, 4.0}, {2, 3, 3.0}});
    expect_pairs(model.quadratic_terms(2, 1), {{2, 3, 1.0}});
}

TEST(QuadraticTest, Unchanged)
{
    Model model = with_functions(2, 3);
    add(model, single(1, {{0, 1, kLarge}}));
    const Quadratic unknown{
        .functions = {0, 2},
        .starts = {0, 1, 2},
        .terms = {{0, 0, 1.0}, {0, 2, 1.0}},
    };
    EXPECT_NE(add_refusal(model, unknown), "");
    const Quadratic infinite{
        .functions = {2, 1},
        .starts = {0, 1, 2},
        .terms = {{1, 1, 1.0}, {1, 0, kInfinity}},
    };
    EXPECT_NE(add_refusal(model, infinite), "");
    const Quadratic overflow{
        .functions = {0, 1},
        .starts = {0, 1, 2},
        .terms = {{1, 1, 1.0}, {1, 0, kLarge}},
    };
    EXPECT_NE(add_refusal(model, overflow), "");
    expect_pairs(model.quadratic_terms(0, 1), {});
    expect_pairs(model.quadratic_terms(1, 1), {{0, 1, kLarge}});
    expect_pairs(model.quadratic_terms(2, 1), {});
}

TEST(QuadraticTest, Atomic)
{
    Model model = with_functions(2, 3);
    add(model, single(0, {{0, 0, kLarge}}));
    const Quadratic overflow{
        .functions = {2, 0},
        .starts = {0, 1, 2},
        .terms = {{1, 1, 1.0}, {0, 0, kLarge}},
    };
    EXPECT_EQ(add_refusal(model, overflow),
              "invalid argument: function 0: sum of coefficients of pair (0, 0) is infinity");
    model.add_functions(std::vector<double>{0.0}, std::vector<Offset>{0, 0}, {});
    expect_pairs(held(model), {{0, 0, kLarge}});
    EXPECT_TRUE(model.quadratic_terms(2, 2).empty());
}

TEST(QuadraticTest, Order)
{
    Model model = with_functions(3, 3);
    const Quadratic large{
        .functions = {1, 2},
        .starts = {0, 1, 2},
        .terms = {{0, 0, kLarge}, {1, 1, kLarge}},
    };
    add(model, large);
    const Quadratic listed{.functions = {1, 1, 5}, .starts = {0, 0, 0, 0}, .terms = {}};
    EXPECT_EQ(add_refusal(model, listed), "invalid argument: function 5: unknown");
    const Quadratic sorted{
        .functions = {1, 0},
        .starts = {0, 2, 3},
        .terms = {{0, 1, 1.0}, {1, 0, 1.0}, {0, 3, 1.0}},
    };
    EXPECT_EQ(add_refusal(model, sorted), "invalid argument: function 1: pair (0, 1) is repeated");
    const Quadratic summed{
        .functions = {1, 0},
        .starts = {0, 1, 2},
        .terms = {{0, 0, kLarge}, {0, 3, 1.0}},
    };
    EXPECT_EQ(add_refusal(model, summed), "invalid argument: function 0: variable 3 is unknown");
    const std::string sum = "invalid argument: function ";
    const Quadratic falling{
        .functions = {2, 1},
        .starts = {0, 1, 2},
        .terms = {{1, 1, kLarge}, {0, 0, kLarge}},
    };
    const std::string second = "2: sum of coefficients of pair (1, 1) is infinity";
    EXPECT_EQ(add_refusal(model, falling), sum + second);
    const Quadratic rising{
        .functions = {1, 2},
        .starts = {0, 1, 2},
        .terms = {{0, 0, kLarge}, {1, 1, kLarge}},
    };
    const std::string first = "1: sum of coefficients of pair (0, 0) is infinity";
    EXPECT_EQ(add_refusal(model, rising), sum + first);
    expect_pairs(held(model), {{0, 0, kLarge}, {1, 1, kLarge}});
}

TEST(QuadraticTest, Later)
{
    Model model = with_functions(2, 2);
    add(model, single(0, {{0, 1, 1.0}}));
    const std::vector<double> constants{2.0, 3.0};
    const std::vector<Offset> starts{0, 1, 1};
    model.add_functions(constants, starts, {LinearTerm{.variable = 1, .coefficient = 5.0}});
    expect_pairs(model.quadratic_terms(0, 4), {{0, 1, 1.0}});
    expect_pairs(model.quadratic_terms(2, 2), {});
    add(model, single(2, {{1, 1, 2.0}}));
    expect_pairs(model.quadratic_terms(0, 1), {{0, 1, 1.0}});
    expect_pairs(model.quadratic_terms(2, 1), {{1, 1, 2.0}});
    EXPECT_EQ(model.terms(2, 1).size(), 1U);
    EXPECT_TRUE(model.quadratic_terms(3, 1).empty());
}

TEST(QuadraticTest, ReadRange)
{
    Model model = with_functions(2, 3);
    const Quadratic quadratic{
        .functions = {0, 2},
        .starts = {0, 1, 3},
        .terms = {{0, 0, 1.0}, {0, 1, 2.0}, {1, 1, 3.0}},
    };
    add(model, quadratic);
    expect_pairs(model.quadratic_terms(1, 2), {{0, 1, 2.0}, {1, 1, 3.0}});
    EXPECT_TRUE(model.quadratic_terms(1, 1).empty());
    EXPECT_TRUE(model.quadratic_terms(3, 0).empty());
    EXPECT_EQ(refusal([&] { model.quadratic_terms(-1, 1); }),
              "invalid argument: function -1: unknown");
    EXPECT_EQ(refusal([&] { model.quadratic_terms(1, 3); }),
              "invalid argument: function 3: unknown");
    EXPECT_EQ(refusal([&] { model.quadratic_terms(0, -1); }),
              "invalid argument: argument 'count': -1 is negative");
}

TEST(QuadraticTest, BadStarts)
{
    Model model = with_functions(2, 2);
    const Quadratic late{.functions = {0}, .starts = {1, 1}, .terms = {{0, 0, 1.0}}};
    EXPECT_EQ(add_refusal(model, late), "invalid argument: argument 'starts': begins at 1, not 0");
    const Quadratic falling{.functions = {0, 1}, .starts = {0, 2, 1}, .terms = {{0, 0, 1.0}}};
    EXPECT_EQ(add_refusal(model, falling),
              "invalid argument: argument 'starts': decreases from 2 to 1 at entry 2");
    EXPECT_TRUE(held(model).empty());
}

TEST(QuadraticTest, Mismatch)
{
    Model model = with_functions(1, 1);
    const Quadratic short_starts{.functions = {0}, .starts = {0}, .terms = {}};
    expect_internal([&] { add(model, short_starts); });
    const Quadratic long_terms{.functions = {0}, .starts = {0, 2}, .terms = {{0, 0, 1.0}}};
    expect_internal([&] { add(model, long_terms); });
    EXPECT_TRUE(held(model).empty());
}

TEST(QuadraticTest, Room)
{
    Model model = with_functions(1, 1);
    const Offset most = (std::numeric_limits<Offset>::max)();
    const auto call = [&] { model.check_quadratic_room(most); };
    EXPECT_EQ(failure(call, biyos::ErrorKind::NoMemory),
              "out of memory: model: no quadratic term offset left");
    const Offset large = Offset{1} << 59;
    const auto beyond = [&] { model.check_quadratic_room(large); };
    EXPECT_EQ(failure(beyond, biyos::ErrorKind::NoMemory),
              "out of memory: model: no quadratic term offset left");
    model.check_quadratic_room(1);
    add(model, single(0, {{0, 0, 1.0}}));
    expect_pairs(held(model), {{0, 0, 1.0}});
}

TEST(QuadraticTest, Moved)
{
    Model model = with_functions(1, 1);
    add(model, single(0, {{0, 0, 2.0}}));
    const Model taken = std::move(model);
    expect_pairs(held(taken), {{0, 0, 2.0}});
    EXPECT_TRUE(model.quadratic_terms(0, 0).empty());
    model.add_variables(std::vector<Variable>(1, taken.variable(0)));
    model.add_functions(std::vector<double>{0.0}, std::vector<Offset>{0, 0}, {});
    add(model, single(0, {{0, 0, 3.0}}));
    expect_pairs(held(model), {{0, 0, 3.0}});
}
