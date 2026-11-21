#include "core/error.hpp"
#include "core/index.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"
#include "model/transpose.hpp"
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

void add_terms(Model &mdl, const Linear &lin)
{
    mdl.add_lin_terms(lin.functions, lin.starts, lin.terms);
}

Linear terms_of(Index fun, std::vector<LinearTerm> terms)
{
    const auto size = static_cast<Offset>(terms.size());
    return Linear{.functions = {fun}, .starts = {0, size}, .terms = std::move(terms)};
}

std::string terms_refusal(Model &mdl, const Linear &lin)
{
    return refusal([&] { add_terms(mdl, lin); });
}

ByVariable read_vars(const Model &mdl, Index beg, Index n)
{
    ByVariable read{.starts = biyos::var_starts(mdl, beg, n), .functions = {}, .values = {}};
    const auto size = static_cast<std::size_t>(read.starts.back());
    read.functions.assign(size, -1);
    read.values.assign(size, kNan);
    biyos::var_terms(mdl, beg, read.starts, read.functions, read.values);
    return read;
}

void expect_vars(const ByVariable &found, const ByVariable &wanted)
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
    Model mdl = with_vars(4);
    const Functions funs{
        .constants = {1.0, 2.0, 3.0},
        .starts = {0, 2, 2, 3},
        .terms = {{1, 1.0}, {3, 2.0}, {2, 3.0}},
    };
    add(mdl, funs);
    return mdl;
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

TEST(FunctionTest, LinearTerms)
{
    Model mdl = with_terms();
    const Linear lin{
        .functions = {2, 0},
        .starts = {0, 2, 5},
        .terms = {{0, -1.0}, {2, 0.5}, {3, 0.25}, {0, 4.0}, {2, 5.0}},
    };
    add_terms(mdl, lin);
    expect_terms(mdl.terms(0, 1), {{0, 4.0}, {1, 1.0}, {2, 5.0}, {3, 2.25}});
    expect_terms(mdl.terms(1, 1), {});
    expect_terms(mdl.terms(2, 1), {{0, -1.0}, {2, 3.5}});
    expect_constants(mdl.constants(0, 3), {1.0, 2.0, 3.0});
}

TEST(FunctionTest, LinearMerge)
{
    Model mdl = with_vars(5);
    const Functions held{
        .constants = {0.0, 0.0, 0.0, 0.0, 0.0},
        .starts = {0, 1, 2, 4, 4, 5},
        .terms = {{0, 1.0}, {1, 2.0}, {0, 3.0}, {2, 4.0}, {4, 5.0}},
    };
    add(mdl, held);
    const Linear lin{
        .functions = {3, 0, 4, 2},
        .starts = {0, 3, 4, 6, 7},
        .terms = {{3, 1.0}, {0, 1.0}, {1, 1.0}, {0, 1.0}, {4, 1.0}, {3, 1.0}, {1, 1.0}},
    };
    add_terms(mdl, lin);
    Model whole = with_vars(5);
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
    EXPECT_EQ(difference(mdl, whole), "");
}

TEST(FunctionTest, LinearTwice)
{
    Model mdl = with_terms();
    add_terms(mdl, terms_of(1, {{3, 1.0}, {1, 2.0}}));
    add_terms(mdl, terms_of(1, {{2, 4.0}, {1, 0.5}, {0, 3.0}}));
    expect_terms(mdl.terms(0, 1), {{1, 1.0}, {3, 2.0}});
    expect_terms(mdl.terms(1, 1), {{0, 3.0}, {1, 2.5}, {2, 4.0}, {3, 1.0}});
    expect_terms(mdl.terms(2, 1), {{2, 3.0}});
}

TEST(FunctionTest, LinearRefused)
{
    Model mdl = with_terms();
    const std::string bad = "invalid argument: ";
    const Linear twice{
        .functions = {1, 0, 1},
        .starts = {0, 1, 1, 2},
        .terms = {{0, 1.0}, {1, 2.0}},
    };
    EXPECT_EQ(terms_refusal(mdl, twice), bad + "argument 'funidx': function 1 is repeated");
    EXPECT_EQ(terms_refusal(mdl, terms_of(3, {})), bad + "function 3: unknown");
    EXPECT_EQ(terms_refusal(mdl, terms_of(-1, {{0, 1.0}})), bad + "function -1: unknown");
    EXPECT_EQ(terms_refusal(mdl, terms_of(0, {{0, 1.0}, {4, 1.0}})),
              bad + "function 0: variable 4 is unknown");
    EXPECT_EQ(terms_refusal(mdl, terms_of(0, {{-1, 1.0}})),
              bad + "function 0: variable -1 is unknown");
    EXPECT_EQ(terms_refusal(mdl, terms_of(0, {{1, 1.0}, {0, 1.0}, {1, 2.0}})),
              bad + "function 0: variable 1 is repeated");
    const std::string coefficient = bad + "function 2: coefficient of variable 2 ";
    EXPECT_EQ(terms_refusal(mdl, terms_of(2, {{0, 1.0}, {2, kNan}})), coefficient + "is NaN");
    EXPECT_EQ(terms_refusal(mdl, terms_of(2, {{2, kInfinity}})), coefficient + "is infinity");
    EXPECT_EQ(terms_refusal(mdl, terms_of(2, {{2, -kInfinity}})),
              coefficient + "is minus infinity");
    EXPECT_EQ(difference(mdl, with_terms()), "");
}

TEST(FunctionTest, LinearSums)
{
    Model mdl = with_vars(2);
    add(mdl, single(0.0, {{0, kLarge}, {1, -kLarge}}));
    const std::string bad = "invalid argument: function 0: sum of coefficients of variable ";
    EXPECT_EQ(terms_refusal(mdl, terms_of(0, {{0, kLarge}})), bad + "0 is infinity");
    EXPECT_EQ(terms_refusal(mdl, terms_of(0, {{0, 1.0}, {1, -kLarge}})),
              bad + "1 is minus infinity");
    expect_terms(mdl.terms(0, 1), {{0, kLarge}, {1, -kLarge}});
}

TEST(FunctionTest, LinearAtomic)
{
    Model mdl = with_terms();
    add_terms(mdl, terms_of(2, {{0, kLarge}}));
    const Model before = mdl;
    const Linear overflow{
        .functions = {0, 2},
        .starts = {0, 1, 2},
        .terms = {{0, 1.0}, {0, kLarge}},
    };
    EXPECT_EQ(terms_refusal(mdl, overflow),
              "invalid argument: function 2: sum of coefficients of variable 0 is infinity");
    EXPECT_EQ(difference(mdl, before), "");
    add(mdl, single(4.0, {{3, 1.0}}));
    expect_terms(mdl.terms(2, 2), {{0, kLarge}, {2, 3.0}, {3, 1.0}});
}

TEST(FunctionTest, LinearZeros)
{
    Model mdl = with_terms();
    add_terms(mdl, terms_of(0, {{1, -1.0}, {0, 0.0}, {2, -0.0}}));
    expect_exact(mdl.terms(0, 1), {{0, 0.0}, {1, 0.0}, {2, -0.0}, {3, 2.0}});
    add_terms(mdl, terms_of(1, {{2, 1.5}, {3, -0.0}}));
    add_terms(mdl, terms_of(1, {{2, -1.5}, {3, -0.0}}));
    expect_exact(mdl.terms(1, 1), {{2, 0.0}, {3, -0.0}});
}

TEST(FunctionTest, LinearEmpty)
{
    Model mdl = with_terms();
    add_terms(mdl, Linear{.functions = {}, .starts = {0}, .terms = {}});
    add_terms(mdl, Linear{.functions = {2, 0}, .starts = {0, 0, 0}, .terms = {}});
    EXPECT_EQ(difference(mdl, with_terms()), "");
}

TEST(FunctionTest, LinearLater)
{
    Model mdl = with_terms();
    add_terms(mdl, terms_of(1, {{0, 1.0}}));
    add(mdl, single(4.0, {{2, 2.0}}));
    expect_terms(mdl.terms(1, 1), {{0, 1.0}});
    expect_terms(mdl.terms(3, 1), {{2, 2.0}});
    add_terms(mdl, terms_of(3, {{0, 1.0}}));
    expect_terms(mdl.terms(2, 2), {{2, 3.0}, {0, 1.0}, {2, 2.0}});
    EXPECT_EQ(mdl.terms(0, 4).size(), 6U);
}

TEST(FunctionTest, LinearStarts)
{
    Model mdl = with_terms();
    const Linear late{.functions = {0}, .starts = {1, 1}, .terms = {{0, 1.0}}};
    EXPECT_EQ(terms_refusal(mdl, late), "invalid argument: argument 'off': begins at 1, not 0");
    const Linear falling{.functions = {0, 1}, .starts = {0, 2, 1}, .terms = {{0, 1.0}}};
    EXPECT_EQ(terms_refusal(mdl, falling),
              "invalid argument: argument 'off': decreases from 2 to 1 at entry 2");
    const Linear short_starts{.functions = {0}, .starts = {0}, .terms = {}};
    expect_internal([&] { add_terms(mdl, short_starts); });
    const Linear long_terms{.functions = {0}, .starts = {0, 2}, .terms = {{0, 1.0}}};
    expect_internal([&] { add_terms(mdl, long_terms); });
    EXPECT_EQ(difference(mdl, with_terms()), "");
}

TEST(FunctionTest, LinearCopy)
{
    Model mdl = with_terms();
    add_terms(mdl, terms_of(1, {{2, 1.5}, {0, -0.0}}));
    Model copy = mdl;
    EXPECT_EQ(difference(mdl, copy), "");
    add_terms(copy, terms_of(1, {{2, 1.0}}));
    EXPECT_EQ(difference(mdl, copy), "function 1: term 1: coefficient 1.5 against 2.5");
    expect_exact(mdl.terms(1, 1), {{0, -0.0}, {2, 1.5}});
}

TEST(FunctionTest, VarTerms)
{
    Model mdl = with_terms();
    add_terms(mdl, terms_of(1, {{3, 4.0}, {1, -1.0}}));
    const ByVariable wanted{
        .starts = {0, 0, 2, 3, 5},
        .functions = {0, 1, 2, 0, 1},
        .values = {1.0, -1.0, 3.0, 2.0, 4.0},
    };
    expect_vars(read_vars(mdl, 0, 4), wanted);
}

TEST(FunctionTest, VarRange)
{
    Model mdl = with_terms();
    add_terms(mdl, terms_of(1, {{3, 4.0}, {1, -1.0}}));
    const ByVariable middle{.starts = {0, 2, 3},
                            .functions = {0, 1, 2},
                            .values = {1.0, -1.0, 3.0}};
    expect_vars(read_vars(mdl, 1, 2), middle);
    const ByVariable last{.starts = {0, 2}, .functions = {0, 1}, .values = {2.0, 4.0}};
    expect_vars(read_vars(mdl, 3, 1), last);
    expect_vars(read_vars(mdl, 0, 1), ByVariable{.starts = {0, 0}, .functions = {}, .values = {}});
    expect_vars(read_vars(mdl, 4, 0), ByVariable{.starts = {0}, .functions = {}, .values = {}});
    expect_vars(read_vars(with_vars(0), 0, 0),
                ByVariable{.starts = {0}, .functions = {}, .values = {}});
}

TEST(FunctionTest, VarRefused)
{
    const Model mdl = with_terms();
    EXPECT_EQ(refusal([&] { biyos::var_starts(mdl, -1, 1); }),
              "invalid argument: variable -1: unknown");
    EXPECT_EQ(refusal([&] { biyos::var_starts(mdl, 2, 3); }),
              "invalid argument: variable 4: unknown");
    EXPECT_EQ(refusal([&] { biyos::var_starts(mdl, 5, 0); }),
              "invalid argument: variable 5: unknown");
    EXPECT_EQ(refusal([&] { biyos::var_starts(mdl, 0, -1); }),
              "invalid argument: argument 'n': -1 is negative");
}

TEST(FunctionTest, VarSized)
{
    const Model mdl = with_terms();
    const std::vector<Offset> starts = biyos::var_starts(mdl, 0, 4);
    EXPECT_EQ(starts, (std::vector<Offset>{0, 0, 1, 2, 3}));
    std::vector<Index> funs(2, -1);
    std::vector<double> values(3, kInfinity);
    biyos::var_terms(mdl, 0, starts, funs, {});
    EXPECT_EQ(funs, (std::vector<Index>{0, 2}));
    biyos::var_terms(mdl, 0, starts, {}, values);
    expect_constants(values, {1.0, 3.0, 2.0});
    funs.assign(2, -1);
    biyos::var_terms(mdl, 0, starts, std::span(funs).first(1), {});
    EXPECT_EQ(funs, (std::vector<Index>{0, -1}));
    biyos::var_terms(mdl, 0, starts, {}, {});
}
