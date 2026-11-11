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

Model with_variables(std::size_t count)
{
    const Variable free{
        .kind = biyos::VariableKind::Continuous,
        .lower = -kInfinity,
        .upper = kInfinity,
    };
    Model model;
    model.add_variables(std::vector<Variable>(count, free));
    return model;
}

void add(Model &model, const Functions &functions)
{
    model.add_functions(functions.constants, functions.starts, functions.terms);
}

Functions single(double constant, std::vector<LinearTerm> terms)
{
    const auto size = static_cast<Offset>(terms.size());
    return Functions{.constants = {constant}, .starts = {0, size}, .terms = std::move(terms)};
}

std::string add_refusal(Model &model, const Functions &functions)
{
    return refusal([&] { add(model, functions); });
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
    Model model = with_variables(4);
    const Functions functions{
        .constants = {0.0, 0.0},
        .starts = {0, 3, 5},
        .terms = {{3, 1.5}, {0, -2.0}, {2, 0.5}, {1, 4.0}, {0, 1.0}},
    };
    add(model, functions);
    ASSERT_EQ(model.function_count(), 2);
    expect_terms(model.terms(0, 1), {{0, -2.0}, {2, 0.5}, {3, 1.5}});
    expect_terms(model.terms(1, 1), {{0, 1.0}, {1, 4.0}});
}

TEST(FunctionTest, Constants)
{
    Model model = with_variables(1);
    const Functions functions{
        .constants = {2.5, 0.0, -1.0e300},
        .starts = {0, 1, 1, 1},
        .terms = {{0, 1.0}},
    };
    add(model, functions);
    expect_constants(model.constants(0, 3), {2.5, 0.0, -1.0e300});
    expect_terms(model.terms(0, 1), {{0, 1.0}});
}

TEST(FunctionTest, BadConstants)
{
    Model model = with_variables(1);
    const std::string refused = "invalid argument: function 0: constant ";
    EXPECT_EQ(add_refusal(model, single(kNan, {})), refused + "is NaN");
    EXPECT_EQ(add_refusal(model, single(kInfinity, {{0, 1.0}})), refused + "is infinity");
    EXPECT_EQ(add_refusal(model, single(-kInfinity, {})), refused + "is minus infinity");
    EXPECT_EQ(model.function_count(), 0);
}

TEST(FunctionTest, UnknownVariable)
{
    Model model = with_variables(3);
    add(model, single(0.0, {{2, 1.0}}));
    EXPECT_EQ(add_refusal(model, single(0.0, {{0, 1.0}, {3, 1.0}})),
              "invalid argument: function 1: variable 3 is unknown");
    EXPECT_EQ(add_refusal(model, single(0.0, {{-1, 1.0}})),
              "invalid argument: function 1: variable -1 is unknown");
    EXPECT_EQ(model.function_count(), 1);
}

TEST(FunctionTest, Repeated)
{
    Model model = with_variables(3);
    EXPECT_EQ(add_refusal(model, single(0.0, {{2, 1.0}, {0, 1.0}, {2, 3.0}})),
              "invalid argument: function 0: variable 2 is repeated");
    EXPECT_EQ(model.function_count(), 0);
}

TEST(FunctionTest, Coefficients)
{
    Model model = with_variables(2);
    const std::string refused = "invalid argument: function 0: coefficient of variable 1 ";
    EXPECT_EQ(add_refusal(model, single(0.0, {{0, 1.0}, {1, kNan}})), refused + "is NaN");
    EXPECT_EQ(add_refusal(model, single(0.0, {{1, kInfinity}})), refused + "is infinity");
    EXPECT_EQ(add_refusal(model, single(0.0, {{1, -kInfinity}})), refused + "is minus infinity");
    EXPECT_EQ(model.function_count(), 0);
}

TEST(FunctionTest, Zeros)
{
    Model model = with_variables(3);
    add(model, single(0.0, {{2, 0.0}, {0, 1.0}, {1, -0.0}}));
    expect_terms(model.terms(0, 1), {{0, 1.0}, {1, 0.0}, {2, 0.0}});
}

TEST(FunctionTest, BadStarts)
{
    Model model = with_variables(2);
    const Functions late{.constants = {0.0}, .starts = {1, 1}, .terms = {{0, 1.0}}};
    EXPECT_EQ(add_refusal(model, late), "invalid argument: argument 'starts': begins at 1, not 0");
    const Functions falling{.constants = {0.0, 0.0}, .starts = {0, 2, 1}, .terms = {{0, 1.0}}};
    EXPECT_EQ(add_refusal(model, falling),
              "invalid argument: argument 'starts': decreases from 2 to 1 at entry 2");
    EXPECT_EQ(model.function_count(), 0);
}

TEST(FunctionTest, Appended)
{
    Model model = with_variables(3);
    add(model, single(1.0, {{1, 1.0}, {0, 2.0}}));
    const Functions more{
        .constants = {2.0, 3.0},
        .starts = {0, 0, 2},
        .terms = {{2, 3.0}, {1, 4.0}},
    };
    add(model, more);
    ASSERT_EQ(model.function_count(), 3);
    expect_constants(model.constants(0, 3), {1.0, 2.0, 3.0});
    expect_terms(model.terms(0, 3), {{0, 2.0}, {1, 1.0}, {1, 4.0}, {2, 3.0}});
    expect_terms(model.terms(1, 1), {});
    expect_terms(model.terms(2, 1), {{1, 4.0}, {2, 3.0}});
}

TEST(FunctionTest, Unchanged)
{
    Model model = with_variables(2);
    add(model, single(1.0, {{1, 1.0}}));
    const Functions repeated{
        .constants = {2.0, 3.0},
        .starts = {0, 1, 3},
        .terms = {{0, 1.0}, {1, 2.0}, {1, 3.0}},
    };
    EXPECT_NE(add_refusal(model, repeated), "");
    const Functions infinite{
        .constants = {2.0, kInfinity},
        .starts = {0, 1, 1},
        .terms = {{0, 1.0}},
    };
    EXPECT_NE(add_refusal(model, infinite), "");
    ASSERT_EQ(model.function_count(), 1);
    expect_constants(model.constants(0, 1), {1.0});
    expect_terms(model.terms(0, 1), {{1, 1.0}});
}

TEST(FunctionTest, ReadRange)
{
    Model model = with_variables(2);
    const Functions functions{
        .constants = {1.0, 2.0, 3.0},
        .starts = {0, 1, 3, 4},
        .terms = {{0, 1.0}, {1, 2.0}, {0, 3.0}, {1, 4.0}},
    };
    add(model, functions);
    expect_constants(model.constants(1, 2), {2.0, 3.0});
    expect_terms(model.terms(1, 2), {{0, 3.0}, {1, 2.0}, {1, 4.0}});
    EXPECT_TRUE(model.constants(3, 0).empty());
    EXPECT_TRUE(model.terms(3, 0).empty());
    EXPECT_EQ(refusal([&] { model.constants(-1, 1); }), "invalid argument: function -1: unknown");
    EXPECT_EQ(refusal([&] { model.terms(1, 3); }), "invalid argument: function 3: unknown");
    EXPECT_EQ(refusal([&] { model.terms(4, 0); }), "invalid argument: function 4: unknown");
    EXPECT_EQ(refusal([&] { model.terms(0, -1); }),
              "invalid argument: argument 'count': -1 is negative");
}

TEST(FunctionTest, Mismatch)
{
    Model model = with_variables(1);
    const Functions short_starts{.constants = {0.0}, .starts = {0}, .terms = {}};
    expect_internal([&] { add(model, short_starts); });
    const Functions long_terms{.constants = {0.0}, .starts = {0, 2}, .terms = {}};
    expect_internal([&] { add(model, long_terms); });
    EXPECT_EQ(model.function_count(), 0);
}

TEST(FunctionTest, Room)
{
    Model model = with_variables(1);
    const Offset most = (std::numeric_limits<Offset>::max)();
    const auto call = [&] { model.check_room(1, most); };
    EXPECT_EQ(failure(call, biyos::ErrorKind::NoMemory),
              "out of memory: model: no term offset left");
    const Offset large = Offset{1} << 59;
    const auto beyond = [&] { model.check_room(1, large); };
    EXPECT_EQ(failure(beyond, biyos::ErrorKind::NoMemory),
              "out of memory: model: no term offset left");
    model.check_room(1, 1);
    add(model, single(0.0, {{0, 1.0}}));
    EXPECT_EQ(model.function_count(), 1);
}

TEST(FunctionTest, Moved)
{
    Model model = with_variables(1);
    add(model, single(1.0, {{0, 2.0}}));
    const Model taken = std::move(model);
    expect_terms(taken.terms(0, 1), {{0, 2.0}});
    EXPECT_EQ(model.function_count(), 0);
    EXPECT_TRUE(model.terms(0, 0).empty());
    add(model, single(0.0, {}));
    expect_terms(model.terms(0, 1), {});
    Model assigned;
    assigned = std::move(model);
    add(model, single(0.0, {}));
    EXPECT_EQ(model.function_count(), 1);
    EXPECT_EQ(assigned.function_count(), 1);
}
