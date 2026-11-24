#include "core/error.hpp"
#include "core/index.hpp"
#include "model/compare/compare.hpp"
#include "model/functions/transpose.hpp"
#include "model/model.hpp"
#include "refusal.hpp"

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
using biyos::Variable;
using biyos::test::failure;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kLarge = 1.0e308;

struct Functions
{
    std::vector<double> constants;
    std::vector<Offset> starts;
    std::vector<LinearTerm> terms;
};

struct Linear
{
    std::vector<Index> functions;
    std::vector<Offset> starts;
    std::vector<LinearTerm> terms;
};

struct ByVariable
{
    std::vector<Offset> starts;
    std::vector<Index> functions;
    std::vector<double> values;
};

Model with_variables(std::size_t n)
{
    const Variable free{
        .kind = biyos::VariableKind::Continuous,
        .lower = -kInfinity,
        .upper = kInfinity,
    };
    Model model;
    model.add_variables(std::vector<Variable>(n, free));
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

std::uint64_t bits_of(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

void expect_exact(std::span<const LinearTerm> found, const std::vector<LinearTerm> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        const double coefficient = found[k].coefficient;
        EXPECT_EQ(found[k].variable, wanted[k].variable) << "term " << k;
        EXPECT_EQ(bits_of(coefficient), bits_of(wanted[k].coefficient)) << coefficient;
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

void add_terms(Model &model, const Linear &linear)
{
    model.add_linear_terms(linear.functions, linear.starts, linear.terms);
}

Linear terms_of(Index function, std::vector<LinearTerm> terms)
{
    const auto size = static_cast<Offset>(terms.size());
    return Linear{.functions = {function}, .starts = {0, size}, .terms = std::move(terms)};
}

std::string term_refusal(Model &model, const Linear &linear)
{
    return refusal([&] { add_terms(model, linear); });
}

ByVariable read_variables(const Model &model, Index start, Index size)
{
    ByVariable read{.starts = biyos::variable_starts(model, start, size),
                    .functions = {},
                    .values = {}};
    const auto total = static_cast<std::size_t>(read.starts.back());
    read.functions.assign(total, -1);
    read.values.assign(total, kNan);
    biyos::variable_terms(model, start, read.starts, read.functions, read.values);
    return read;
}

void expect_variables(const ByVariable &found, const ByVariable &wanted)
{
    EXPECT_EQ(found.starts, wanted.starts);
    EXPECT_EQ(found.functions, wanted.functions);
    ASSERT_EQ(found.values.size(), wanted.values.size());
    for (std::size_t k = 0; k < found.values.size(); ++k)
    {
        EXPECT_DOUBLE_EQ(found.values[k], wanted.values[k]) << "term " << k;
    }
}

Model with_terms()
{
    Model model = with_variables(4);
    const Functions functions{
        .constants = {1.0, 2.0, 3.0},
        .starts = {0, 2, 2, 3},
        .terms = {{1, 1.0}, {3, 2.0}, {2, 3.0}},
    };
    add(model, functions);
    return model;
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
    ASSERT_EQ(model.n_functions(), 2);
    expect_terms(model.linear_terms(0, 1), {{0, -2.0}, {2, 0.5}, {3, 1.5}});
    expect_terms(model.linear_terms(1, 1), {{0, 1.0}, {1, 4.0}});
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
    expect_terms(model.linear_terms(0, 1), {{0, 1.0}});
}

TEST(FunctionTest, BadConstants)
{
    Model model = with_variables(1);
    const std::string refused = "invalid argument: function 0: constant ";
    EXPECT_EQ(add_refusal(model, single(kNan, {})), refused + "is NaN");
    EXPECT_EQ(add_refusal(model, single(kInfinity, {{0, 1.0}})), refused + "is infinity");
    EXPECT_EQ(add_refusal(model, single(-kInfinity, {})), refused + "is minus infinity");
    EXPECT_EQ(model.n_functions(), 0);
}

TEST(FunctionTest, UnknownVariable)
{
    Model model = with_variables(3);
    add(model, single(0.0, {{2, 1.0}}));
    EXPECT_EQ(add_refusal(model, single(0.0, {{0, 1.0}, {3, 1.0}})),
              "invalid argument: function 1: variable 3 is unknown");
    EXPECT_EQ(add_refusal(model, single(0.0, {{-1, 1.0}})),
              "invalid argument: function 1: variable -1 is unknown");
    EXPECT_EQ(model.n_functions(), 1);
}

TEST(FunctionTest, Repeated)
{
    Model model = with_variables(3);
    EXPECT_EQ(add_refusal(model, single(0.0, {{2, 1.0}, {0, 1.0}, {2, 3.0}})),
              "invalid argument: function 0: variable 2 is repeated");
    EXPECT_EQ(model.n_functions(), 0);
}

TEST(FunctionTest, Coefficients)
{
    Model model = with_variables(2);
    const std::string refused = "invalid argument: function 0: coefficient of variable 1 ";
    EXPECT_EQ(add_refusal(model, single(0.0, {{0, 1.0}, {1, kNan}})), refused + "is NaN");
    EXPECT_EQ(add_refusal(model, single(0.0, {{1, kInfinity}})), refused + "is infinity");
    EXPECT_EQ(add_refusal(model, single(0.0, {{1, -kInfinity}})), refused + "is minus infinity");
    EXPECT_EQ(model.n_functions(), 0);
}

TEST(FunctionTest, Zeros)
{
    Model model = with_variables(3);
    add(model, single(0.0, {{2, 0.0}, {0, 1.0}, {1, -0.0}}));
    expect_terms(model.linear_terms(0, 1), {{0, 1.0}, {1, 0.0}, {2, 0.0}});
}

TEST(FunctionTest, BadStarts)
{
    Model model = with_variables(2);
    const Functions late{.constants = {0.0}, .starts = {1, 1}, .terms = {{0, 1.0}}};
    EXPECT_EQ(add_refusal(model, late), "invalid argument: argument 'off': begins at 1, not 0");
    const Functions falling{.constants = {0.0, 0.0}, .starts = {0, 2, 1}, .terms = {{0, 1.0}}};
    EXPECT_EQ(add_refusal(model, falling),
              "invalid argument: argument 'off': decreases from 2 to 1 at entry 2");
    EXPECT_EQ(model.n_functions(), 0);
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
    ASSERT_EQ(model.n_functions(), 3);
    expect_constants(model.constants(0, 3), {1.0, 2.0, 3.0});
    expect_terms(model.linear_terms(0, 3), {{0, 2.0}, {1, 1.0}, {1, 4.0}, {2, 3.0}});
    expect_terms(model.linear_terms(1, 1), {});
    expect_terms(model.linear_terms(2, 1), {{1, 4.0}, {2, 3.0}});
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
    ASSERT_EQ(model.n_functions(), 1);
    expect_constants(model.constants(0, 1), {1.0});
    expect_terms(model.linear_terms(0, 1), {{1, 1.0}});
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
    expect_terms(model.linear_terms(1, 2), {{0, 3.0}, {1, 2.0}, {1, 4.0}});
    EXPECT_TRUE(model.constants(3, 0).empty());
    EXPECT_TRUE(model.linear_terms(3, 0).empty());
    EXPECT_EQ(refusal([&] { model.constants(-1, 1); }), "invalid argument: function -1: unknown");
    EXPECT_EQ(refusal([&] { model.linear_terms(1, 3); }), "invalid argument: function 3: unknown");
    EXPECT_EQ(refusal([&] { model.linear_terms(4, 0); }), "invalid argument: function 4: unknown");
    EXPECT_EQ(refusal([&] { model.linear_terms(0, -1); }),
              "invalid argument: argument 'n': -1 is negative");
}

TEST(FunctionTest, Mismatch)
{
    Model model = with_variables(1);
    const Functions short_starts{.constants = {0.0}, .starts = {0}, .terms = {}};
    expect_internal([&] { add(model, short_starts); });
    const Functions long_terms{.constants = {0.0}, .starts = {0, 2}, .terms = {}};
    expect_internal([&] { add(model, long_terms); });
    EXPECT_EQ(model.n_functions(), 0);
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
    EXPECT_EQ(model.n_functions(), 1);
}

TEST(FunctionTest, Moved)
{
    Model model = with_variables(1);
    add(model, single(1.0, {{0, 2.0}}));
    const Model taken = std::move(model);
    expect_terms(taken.linear_terms(0, 1), {{0, 2.0}});
    EXPECT_EQ(model.n_functions(), 0);
    EXPECT_TRUE(model.linear_terms(0, 0).empty());
    add(model, single(0.0, {}));
    expect_terms(model.linear_terms(0, 1), {});
    Model assigned;
    assigned = std::move(model);
    add(model, single(0.0, {}));
    EXPECT_EQ(model.n_functions(), 1);
    EXPECT_EQ(assigned.n_functions(), 1);
}

TEST(FunctionTest, LinearTerms)
{
    Model model = with_terms();
    const Linear linear{
        .functions = {2, 0},
        .starts = {0, 2, 5},
        .terms = {{0, -1.0}, {2, 0.5}, {3, 0.25}, {0, 4.0}, {2, 5.0}},
    };
    add_terms(model, linear);
    expect_terms(model.linear_terms(0, 1), {{0, 4.0}, {1, 1.0}, {2, 5.0}, {3, 2.25}});
    expect_terms(model.linear_terms(1, 1), {});
    expect_terms(model.linear_terms(2, 1), {{0, -1.0}, {2, 3.5}});
    expect_constants(model.constants(0, 3), {1.0, 2.0, 3.0});
}

TEST(FunctionTest, LinearMerge)
{
    Model model = with_variables(5);
    const Functions held{
        .constants = {0.0, 0.0, 0.0, 0.0, 0.0},
        .starts = {0, 1, 2, 4, 4, 5},
        .terms = {{0, 1.0}, {1, 2.0}, {0, 3.0}, {2, 4.0}, {4, 5.0}},
    };
    add(model, held);
    const Linear linear{
        .functions = {3, 0, 4, 2},
        .starts = {0, 3, 4, 6, 7},
        .terms = {{3, 1.0}, {0, 1.0}, {1, 1.0}, {0, 1.0}, {4, 1.0}, {3, 1.0}, {1, 1.0}},
    };
    add_terms(model, linear);
    Model whole = with_variables(5);
    const Functions merged{
        .constants = {0.0, 0.0, 0.0, 0.0, 0.0},
        .starts = {0, 1, 2, 5, 8, 10},
        .terms = {{0, 2.0},
                  {1, 2.0},
                  {0, 3.0},
                  {1, 1.0},
                  {2, 4.0},
                  {0, 1.0},
                  {1, 1.0},
                  {3, 1.0},
                  {3, 1.0},
                  {4, 6.0}},
    };
    add(whole, merged);
    EXPECT_EQ(difference(model, whole), "");
}

TEST(FunctionTest, LinearTwice)
{
    Model model = with_terms();
    add_terms(model, terms_of(1, {{3, 1.0}, {1, 2.0}}));
    add_terms(model, terms_of(1, {{2, 4.0}, {1, 0.5}, {0, 3.0}}));
    expect_terms(model.linear_terms(0, 1), {{1, 1.0}, {3, 2.0}});
    expect_terms(model.linear_terms(1, 1), {{0, 3.0}, {1, 2.5}, {2, 4.0}, {3, 1.0}});
    expect_terms(model.linear_terms(2, 1), {{2, 3.0}});
}

TEST(FunctionTest, LinearRefused)
{
    Model model = with_terms();
    const std::string bad = "invalid argument: ";
    const Linear twice{
        .functions = {1, 0, 1},
        .starts = {0, 1, 1, 2},
        .terms = {{0, 1.0}, {1, 2.0}},
    };
    EXPECT_EQ(term_refusal(model, twice), bad + "argument 'funidx': function 1 is repeated");
    EXPECT_EQ(term_refusal(model, terms_of(3, {})), bad + "function 3: unknown");
    EXPECT_EQ(term_refusal(model, terms_of(-1, {{0, 1.0}})), bad + "function -1: unknown");
    EXPECT_EQ(term_refusal(model, terms_of(0, {{0, 1.0}, {4, 1.0}})),
              bad + "function 0: variable 4 is unknown");
    EXPECT_EQ(term_refusal(model, terms_of(0, {{-1, 1.0}})),
              bad + "function 0: variable -1 is unknown");
    EXPECT_EQ(term_refusal(model, terms_of(0, {{1, 1.0}, {0, 1.0}, {1, 2.0}})),
              bad + "function 0: variable 1 is repeated");
    const std::string coefficient = bad + "function 2: coefficient of variable 2 ";
    EXPECT_EQ(term_refusal(model, terms_of(2, {{0, 1.0}, {2, kNan}})), coefficient + "is NaN");
    EXPECT_EQ(term_refusal(model, terms_of(2, {{2, kInfinity}})), coefficient + "is infinity");
    EXPECT_EQ(term_refusal(model, terms_of(2, {{2, -kInfinity}})),
              coefficient + "is minus infinity");
    EXPECT_EQ(difference(model, with_terms()), "");
}

TEST(FunctionTest, LinearSums)
{
    Model model = with_variables(2);
    add(model, single(0.0, {{0, kLarge}, {1, -kLarge}}));
    const std::string bad = "invalid argument: function 0: sum of coefficients of variable ";
    EXPECT_EQ(term_refusal(model, terms_of(0, {{0, kLarge}})), bad + "0 is infinity");
    EXPECT_EQ(term_refusal(model, terms_of(0, {{0, 1.0}, {1, -kLarge}})),
              bad + "1 is minus infinity");
    expect_terms(model.linear_terms(0, 1), {{0, kLarge}, {1, -kLarge}});
}

TEST(FunctionTest, LinearAtomic)
{
    Model model = with_terms();
    add_terms(model, terms_of(2, {{0, kLarge}}));
    const Model before = model;
    const Linear overflow{
        .functions = {0, 2},
        .starts = {0, 1, 2},
        .terms = {{0, 1.0}, {0, kLarge}},
    };
    EXPECT_EQ(term_refusal(model, overflow),
              "invalid argument: function 2: sum of coefficients of variable 0 is infinity");
    EXPECT_EQ(difference(model, before), "");
    add(model, single(4.0, {{3, 1.0}}));
    expect_terms(model.linear_terms(2, 2), {{0, kLarge}, {2, 3.0}, {3, 1.0}});
}

TEST(FunctionTest, LinearZeros)
{
    Model model = with_terms();
    add_terms(model, terms_of(0, {{1, -1.0}, {0, 0.0}, {2, -0.0}}));
    expect_exact(model.linear_terms(0, 1), {{0, 0.0}, {1, 0.0}, {2, -0.0}, {3, 2.0}});
    add_terms(model, terms_of(1, {{2, 1.5}, {3, -0.0}}));
    add_terms(model, terms_of(1, {{2, -1.5}, {3, -0.0}}));
    expect_exact(model.linear_terms(1, 1), {{2, 0.0}, {3, -0.0}});
}

TEST(FunctionTest, LinearEmpty)
{
    Model model = with_terms();
    add_terms(model, Linear{.functions = {}, .starts = {0}, .terms = {}});
    add_terms(model, Linear{.functions = {2, 0}, .starts = {0, 0, 0}, .terms = {}});
    EXPECT_EQ(difference(model, with_terms()), "");
}

TEST(FunctionTest, LinearLater)
{
    Model model = with_terms();
    add_terms(model, terms_of(1, {{0, 1.0}}));
    add(model, single(4.0, {{2, 2.0}}));
    expect_terms(model.linear_terms(1, 1), {{0, 1.0}});
    expect_terms(model.linear_terms(3, 1), {{2, 2.0}});
    add_terms(model, terms_of(3, {{0, 1.0}}));
    expect_terms(model.linear_terms(2, 2), {{2, 3.0}, {0, 1.0}, {2, 2.0}});
    EXPECT_EQ(model.linear_terms(0, 4).size(), 6U);
}

TEST(FunctionTest, LinearStarts)
{
    Model model = with_terms();
    const Linear late{.functions = {0}, .starts = {1, 1}, .terms = {{0, 1.0}}};
    EXPECT_EQ(term_refusal(model, late), "invalid argument: argument 'off': begins at 1, not 0");
    const Linear falling{.functions = {0, 1}, .starts = {0, 2, 1}, .terms = {{0, 1.0}}};
    EXPECT_EQ(term_refusal(model, falling),
              "invalid argument: argument 'off': decreases from 2 to 1 at entry 2");
    const Linear short_starts{.functions = {0}, .starts = {0}, .terms = {}};
    expect_internal([&] { add_terms(model, short_starts); });
    const Linear long_terms{.functions = {0}, .starts = {0, 2}, .terms = {{0, 1.0}}};
    expect_internal([&] { add_terms(model, long_terms); });
    EXPECT_EQ(difference(model, with_terms()), "");
}

TEST(FunctionTest, LinearCopy)
{
    Model model = with_terms();
    add_terms(model, terms_of(1, {{2, 1.5}, {0, -0.0}}));
    Model copy = model;
    EXPECT_EQ(difference(model, copy), "");
    add_terms(copy, terms_of(1, {{2, 1.0}}));
    EXPECT_EQ(difference(model, copy), "function 1: term 1: coefficient 1.5 against 2.5");
    expect_exact(model.linear_terms(1, 1), {{0, -0.0}, {2, 1.5}});
}

TEST(FunctionTest, VariableTerms)
{
    Model model = with_terms();
    add_terms(model, terms_of(1, {{3, 4.0}, {1, -1.0}}));
    const ByVariable wanted{
        .starts = {0, 0, 2, 3, 5},
        .functions = {0, 1, 2, 0, 1},
        .values = {1.0, -1.0, 3.0, 2.0, 4.0},
    };
    expect_variables(read_variables(model, 0, 4), wanted);
}

TEST(FunctionTest, VariableRange)
{
    Model model = with_terms();
    add_terms(model, terms_of(1, {{3, 4.0}, {1, -1.0}}));
    const ByVariable middle{.starts = {0, 2, 3},
                            .functions = {0, 1, 2},
                            .values = {1.0, -1.0, 3.0}};
    expect_variables(read_variables(model, 1, 2), middle);
    const ByVariable last{.starts = {0, 2}, .functions = {0, 1}, .values = {2.0, 4.0}};
    expect_variables(read_variables(model, 3, 1), last);
    expect_variables(read_variables(model, 0, 1),
                     ByVariable{.starts = {0, 0}, .functions = {}, .values = {}});
    expect_variables(read_variables(model, 4, 0),
                     ByVariable{.starts = {0}, .functions = {}, .values = {}});
    expect_variables(read_variables(with_variables(0), 0, 0),
                     ByVariable{.starts = {0}, .functions = {}, .values = {}});
}

TEST(FunctionTest, VariableRefused)
{
    const Model model = with_terms();
    EXPECT_EQ(refusal([&] { biyos::variable_starts(model, -1, 1); }),
              "invalid argument: variable -1: unknown");
    EXPECT_EQ(refusal([&] { biyos::variable_starts(model, 2, 3); }),
              "invalid argument: variable 4: unknown");
    EXPECT_EQ(refusal([&] { biyos::variable_starts(model, 5, 0); }),
              "invalid argument: variable 5: unknown");
    EXPECT_EQ(refusal([&] { biyos::variable_starts(model, 0, -1); }),
              "invalid argument: argument 'n': -1 is negative");
}

TEST(FunctionTest, VariableSized)
{
    const Model model = with_terms();
    const std::vector<Offset> starts = biyos::variable_starts(model, 0, 4);
    EXPECT_EQ(starts, (std::vector<Offset>{0, 0, 1, 2, 3}));
    std::vector<Index> functions(2, -1);
    std::vector<double> values(3, kInfinity);
    biyos::variable_terms(model, 0, starts, functions, {});
    EXPECT_EQ(functions, (std::vector<Index>{0, 2}));
    biyos::variable_terms(model, 0, starts, {}, values);
    expect_constants(values, {1.0, 3.0, 2.0});
    functions.assign(2, -1);
    biyos::variable_terms(model, 0, starts, std::span(functions).first(1), {});
    EXPECT_EQ(functions, (std::vector<Index>{0, -1}));
    biyos::variable_terms(model, 0, starts, {}, {});
}
