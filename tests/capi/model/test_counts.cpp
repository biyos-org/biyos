#include "biyos/biyos.h"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::Model;

using Counts = std::vector<BIYOS_offset>;
using Indices = std::vector<BIYOS_index>;
using Values = std::vector<double>;

struct Terms
{
    Counts counts;
    Indices indices;
    Values values;
};

struct Pairs
{
    Counts counts;
    Indices rows;
    Indices columns;
    Values values;
};

struct Sets
{
    std::vector<BIYOS_int> kinds;
    Counts counts;
    Indices variables;
    Values weights;
};

struct Cones
{
    std::vector<BIYOS_int> kinds;
    Counts counts;
    Indices functions;
    Counts parameter_counts;
    Values parameters;
};

struct Program
{
    Counts counts;
    std::vector<BIYOS_int> codes;
    Counts arguments;
    Values values;
};

std::vector<std::uint64_t> bits_of(const Values &values)
{
    std::vector<std::uint64_t> bits;
    for (const double value : values)
    {
        bits.push_back(std::bit_cast<std::uint64_t>(value));
    }
    return bits;
}

void expect_terms(const Terms &found, const Terms &wanted)
{
    EXPECT_EQ(found.counts, wanted.counts);
    EXPECT_EQ(found.indices, wanted.indices);
    EXPECT_EQ(bits_of(found.values), bits_of(wanted.values));
}

void expect_sets(const Sets &found, const Sets &wanted)
{
    EXPECT_EQ(found.kinds, wanted.kinds);
    EXPECT_EQ(found.counts, wanted.counts);
    EXPECT_EQ(found.variables, wanted.variables);
    EXPECT_EQ(bits_of(found.weights), bits_of(wanted.weights));
}

void expect_cones(const Cones &found, const Cones &wanted)
{
    EXPECT_EQ(found.kinds, wanted.kinds);
    EXPECT_EQ(found.counts, wanted.counts);
    EXPECT_EQ(found.functions, wanted.functions);
    EXPECT_EQ(found.parameter_counts, wanted.parameter_counts);
    EXPECT_EQ(bits_of(found.parameters), bits_of(wanted.parameters));
}

void expect_program(const Program &found, const Program &wanted)
{
    EXPECT_EQ(found.counts, wanted.counts);
    EXPECT_EQ(found.codes, wanted.codes);
    EXPECT_EQ(found.arguments, wanted.arguments);
    EXPECT_EQ(bits_of(found.values), bits_of(wanted.values));
}

Terms blank_terms(std::size_t nnz)
{
    return Terms{
        .counts = Counts(3, -1),
        .indices = Indices(nnz, -1),
        .values = Values(nnz, -1.0),
    };
}

Sets blank_sets(std::size_t nnz)
{
    return Sets{
        .kinds = std::vector<BIYOS_int>(3, -1),
        .counts = Counts(3, -1),
        .variables = Indices(nnz, -1),
        .weights = Values(nnz, -1.0),
    };
}

Cones blank_cones(std::size_t nnz, std::size_t nprm)
{
    return Cones{
        .kinds = std::vector<BIYOS_int>(3, -1),
        .counts = Counts(3, -1),
        .functions = Indices(nnz, -1),
        .parameter_counts = Counts(3, -1),
        .parameters = Values(nprm, -1.0),
    };
}

Program blank_program(std::size_t nnz, std::size_t nval)
{
    return Program{
        .counts = Counts(3, -1),
        .codes = std::vector<BIYOS_int>(nnz, -1),
        .arguments = Counts(nnz, -1),
        .values = Values(nval, -1.0),
    };
}

const Terms kRows{.counts = {2, 0, 1}, .indices = {0, 2, 1}, .values = {1.5, -0.0, 2.25}};

const Terms kColumns{.counts = {1, 0, 2}, .indices = {2, 0, 1}, .values = {-3.0, 0.5, -0.0}};

const Pairs kPairs{
    .counts = {1, 0, 2},
    .rows = {0, 0, 1},
    .columns = {1, 0, 2},
    .values = {2.5, -0.0, 0.75},
};

const Sets kSets{
    .kinds = {BIYOS_SOSKND_TWO, BIYOS_SOSKND_ONE, BIYOS_SOSKND_ONE},
    .counts = {2, 0, 2},
    .variables = {0, 2, 3, 1},
    .weights = {1.0, 2.0, 0.5, 1.5},
};

const Cones kCones{
    .kinds = {BIYOS_CONEKND_POW, BIYOS_CONEKND_SOC, BIYOS_CONEKND_POW},
    .counts = {3, 1, 2},
    .functions = {0, 1, 2, 3, 1, 3},
    .parameter_counts = {2, 0, 1},
    .parameters = {0.25, 0.75, 0.5},
};

const Program kProgram{
    .counts = {2, 2, 3},
    .codes =
        {
            BIYOS_OP_VAR,
            BIYOS_OP_EXP,
            BIYOS_OP_VAR,
            BIYOS_OP_LOG,
            BIYOS_OP_VAR,
            BIYOS_OP_VAR,
            BIYOS_OP_DIV,
        },
    .arguments = {0, 1, 1, 1, 0, 1, 2},
    .values = {},
};

const Indices kListed{1, 2, 3};

void add_model(const Model &model, int variables, int functions)
{
    ASSERT_EQ(BIYOS_addvar(model.get(), variables, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model.get(), functions, nullptr), BIYOS_OK);
}

Terms read_rows(const Model &model)
{
    Terms read = blank_terms(3);
    const BIYOS_status status = BIYOS_getrlintermrg(model.get(),
                                                    1,
                                                    4,
                                                    3,
                                                    read.counts.data(),
                                                    read.indices.data(),
                                                    read.values.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

Terms read_columns(const Model &model)
{
    Terms read = blank_terms(3);
    const BIYOS_status status = BIYOS_getclintermrg(model.get(),
                                                    1,
                                                    4,
                                                    3,
                                                    read.counts.data(),
                                                    read.indices.data(),
                                                    read.values.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

Pairs read_pairs(const Model &model)
{
    Pairs read{
        .counts = Counts(3, -1),
        .rows = Indices(3, -1),
        .columns = Indices(3, -1),
        .values = Values(3, -1.0),
    };
    const BIYOS_status status = BIYOS_getquadtermrg(model.get(),
                                                    1,
                                                    4,
                                                    3,
                                                    read.counts.data(),
                                                    read.rows.data(),
                                                    read.columns.data(),
                                                    read.values.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

Sets read_sets(const Model &model, bool listed)
{
    Sets read = blank_sets(4);
    BIYOS_int *kinds = read.kinds.data();
    BIYOS_offset *counts = read.counts.data();
    BIYOS_index *variables = read.variables.data();
    double *weights = read.weights.data();
    const BIYOS_status status =
        listed
            ? BIYOS_getsossp(model.get(), 3, kListed.data(), 4, kinds, counts, variables, weights)
            : BIYOS_getsosrg(model.get(), 1, 4, 4, kinds, counts, variables, weights);
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

Cones read_cones(const Model &model)
{
    Cones read = blank_cones(6, 3);
    BIYOS_model *held = model.get();
    BIYOS_int *kinds = read.kinds.data();
    BIYOS_index *functions = read.functions.data();
    double *parameters = read.parameters.data();
    EXPECT_EQ(BIYOS_getconerg(held, 1, 4, 6, kinds, read.counts.data(), functions), BIYOS_OK);
    EXPECT_EQ(BIYOS_getconeprmrg(held, 1, 4, 3, read.parameter_counts.data(), parameters),
              BIYOS_OK);
    return read;
}

Cones list_cones(const Model &model)
{
    Cones read = blank_cones(6, 3);
    BIYOS_model *held = model.get();
    const BIYOS_index *listed = kListed.data();
    BIYOS_int *kinds = read.kinds.data();
    BIYOS_index *functions = read.functions.data();
    double *parameters = read.parameters.data();
    EXPECT_EQ(BIYOS_getconesp(held, 3, listed, 6, kinds, read.counts.data(), functions), BIYOS_OK);
    EXPECT_EQ(BIYOS_getconeprmsp(held, 3, listed, 3, read.parameter_counts.data(), parameters),
              BIYOS_OK);
    return read;
}

Program read_program(const Model &model, bool listed)
{
    Program read = blank_program(7, 0);
    BIYOS_offset *counts = read.counts.data();
    BIYOS_int *codes = read.codes.data();
    BIYOS_offset *arguments = read.arguments.data();
    double *values = read.values.data();
    const BIYOS_model *held = model.get();
    const BIYOS_status status =
        listed ? BIYOS_getexprsp(held, 3, kListed.data(), 7, 0, counts, codes, arguments, values)
               : BIYOS_getexprrg(held, 1, 4, 7, 0, counts, codes, arguments, values);
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

}

TEST(CountTest, RowCounts)
{
    const Env env;
    const Model model(env);
    add_model(model, 3, 4);
    const Terms &given = kRows;
    ASSERT_EQ(BIYOS_addrlintermrg(model.get(),
                                  1,
                                  4,
                                  given.counts.data(),
                                  given.indices.data(),
                                  given.values.data()),
              BIYOS_OK);
    expect_terms(read_rows(model), kRows);
}

TEST(CountTest, ColumnCounts)
{
    const Env env;
    const Model model(env);
    add_model(model, 4, 3);
    const Terms &given = kColumns;
    ASSERT_EQ(BIYOS_addclintermrg(model.get(),
                                  1,
                                  4,
                                  given.counts.data(),
                                  given.indices.data(),
                                  given.values.data()),
              BIYOS_OK);
    expect_terms(read_columns(model), kColumns);
}

TEST(CountTest, PairCounts)
{
    const Env env;
    const Model model(env);
    add_model(model, 3, 4);
    ASSERT_EQ(BIYOS_addquadtermrg(model.get(),
                                  1,
                                  4,
                                  kPairs.counts.data(),
                                  kPairs.rows.data(),
                                  kPairs.columns.data(),
                                  kPairs.values.data()),
              BIYOS_OK);
    const Pairs read = read_pairs(model);
    EXPECT_EQ(read.counts, kPairs.counts);
    EXPECT_EQ(read.rows, kPairs.rows);
    EXPECT_EQ(read.columns, kPairs.columns);
    EXPECT_EQ(bits_of(read.values), bits_of(kPairs.values));
}

TEST(CountTest, SetCounts)
{
    const Env env;
    const Model model(env);
    add_model(model, 4, 0);
    const BIYOS_int kind = BIYOS_SOSKND_ONE;
    const BIYOS_offset count = 1;
    const BIYOS_index variable = 3;
    ASSERT_EQ(BIYOS_addsos(model.get(), 1, &kind, &count, &variable, nullptr, nullptr), BIYOS_OK);
    BIYOS_index pos = -1;
    ASSERT_EQ(BIYOS_addsos(model.get(),
                           3,
                           kSets.kinds.data(),
                           kSets.counts.data(),
                           kSets.variables.data(),
                           kSets.weights.data(),
                           &pos),
              BIYOS_OK);
    EXPECT_EQ(pos, 1);
    expect_sets(read_sets(model, false), kSets);
    expect_sets(read_sets(model, true), kSets);
}

TEST(CountTest, ConeCounts)
{
    const Env env;
    const Model model(env);
    add_model(model, 0, 4);
    const BIYOS_int kind = BIYOS_CONEKND_SOC;
    const BIYOS_offset count = 2;
    const BIYOS_index functions[] = {0, 1};
    BIYOS_model *held = model.get();
    ASSERT_EQ(BIYOS_addcone(held, 1, &kind, &count, functions, nullptr, nullptr, nullptr),
              BIYOS_OK);
    BIYOS_index pos = -1;
    ASSERT_EQ(BIYOS_addcone(held,
                            3,
                            kCones.kinds.data(),
                            kCones.counts.data(),
                            kCones.functions.data(),
                            kCones.parameter_counts.data(),
                            kCones.parameters.data(),
                            &pos),
              BIYOS_OK);
    EXPECT_EQ(pos, 1);
    expect_cones(read_cones(model), kCones);
    expect_cones(list_cones(model), kCones);
}

TEST(CountTest, PostfixCounts)
{
    const Env env;
    const Model model(env);
    add_model(model, 2, 4);
    BIYOS_model *held = model.get();
    const BIYOS_offset one = 1;
    const BIYOS_int variable = BIYOS_OP_VAR;
    const BIYOS_offset first = 0;
    ASSERT_EQ(BIYOS_addexpr(held, 1, &one, &variable, &first, nullptr, nullptr), BIYOS_OK);
    BIYOS_offset nodes[] = {-1, -1, -1};
    ASSERT_EQ(BIYOS_addexpr(held,
                            3,
                            kProgram.counts.data(),
                            kProgram.codes.data(),
                            kProgram.arguments.data(),
                            nullptr,
                            nodes),
              BIYOS_OK);
    ASSERT_EQ(BIYOS_attachexprrg(held, 1, 4, nodes), BIYOS_OK);
    expect_program(read_program(model, false), kProgram);
    expect_program(read_program(model, true), kProgram);
}
