#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "handles.hpp"
#include "model/compare/compare.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::Model;

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr BIYOS_offset kNoNode = -7;

struct Pwls
{
    std::vector<BIYOS_offset> counts;
    std::vector<double> x;
    std::vector<double> y;
};

const Pwls kSample{
    .counts = {3, 2},
    .x = {-1.0, 0.5, 2.0, -0.0, 0.1},
    .y = {1.0, -0.0, 3.0, 0.3, 7.0},
};

struct Postfix
{
    std::vector<BIYOS_offset> counts;
    std::vector<int> codes;
    std::vector<BIYOS_offset> arguments;
};

std::vector<std::uint64_t> bits_of(const std::vector<double> &values)
{
    std::vector<std::uint64_t> bits;
    bits.reserve(values.size());
    for (const double value : values)
    {
        bits.push_back(std::bit_cast<std::uint64_t>(value));
    }
    return bits;
}

BIYOS_status add_pwls(const Model &model, const Pwls &pwls, BIYOS_index *pos)
{
    return BIYOS_addpwl(model.get(),
                        static_cast<BIYOS_index>(pwls.counts.size()),
                        pwls.counts.data(),
                        pwls.x.data(),
                        pwls.y.data(),
                        pos);
}

void add_sample(const Model &model)
{
    ASSERT_EQ(add_pwls(model, kSample, nullptr), BIYOS_OK);
}

int n_pwls(const Model &model)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnpwl(model.get(), &n), BIYOS_OK);
    return n;
}

Pwls sized(std::size_t n, BIYOS_offset nnz)
{
    const auto size = static_cast<std::size_t>(nnz);
    return Pwls{
        .counts = std::vector<BIYOS_offset>(n, -1),
        .x = std::vector<double>(size, 9.0),
        .y = std::vector<double>(size, 9.0),
    };
}

BIYOS_status read_range(const Model &model, BIYOS_index beg, BIYOS_offset cap, Pwls &read)
{
    const auto end = beg + static_cast<BIYOS_index>(read.counts.size());
    return BIYOS_getpwlrg(model.get(),
                          beg,
                          end,
                          cap,
                          read.counts.data(),
                          read.x.data(),
                          read.y.data());
}

BIYOS_status read_list(const Model &model,
                       const std::vector<BIYOS_index> &indices,
                       BIYOS_offset cap,
                       Pwls &read)
{
    return BIYOS_getpwlsp(model.get(),
                          static_cast<BIYOS_index>(indices.size()),
                          indices.data(),
                          cap,
                          read.counts.data(),
                          read.x.data(),
                          read.y.data());
}

Pwls read_all(const Model &model)
{
    const int n = n_pwls(model);
    BIYOS_offset nnz = -1;
    EXPECT_EQ(BIYOS_getnpwlptrg(model.get(), 0, n, &nnz), BIYOS_OK);
    Pwls read = sized(static_cast<std::size_t>(n), nnz);
    EXPECT_EQ(read_range(model, 0, nnz, read), BIYOS_OK);
    return read;
}

Pwls read_listed(const Model &model, const std::vector<BIYOS_index> &indices)
{
    const auto n = static_cast<BIYOS_index>(indices.size());
    BIYOS_offset nnz = -1;
    EXPECT_EQ(BIYOS_getnpwlptsp(model.get(), n, indices.data(), &nnz), BIYOS_OK);
    Pwls read = sized(indices.size(), nnz);
    EXPECT_EQ(read_list(model, indices, nnz, read), BIYOS_OK);
    return read;
}

void expect_pwls(const Pwls &found, const Pwls &wanted)
{
    EXPECT_EQ(found.counts, wanted.counts);
    EXPECT_EQ(bits_of(found.x), bits_of(wanted.x));
    EXPECT_EQ(bits_of(found.y), bits_of(wanted.y));
}

Pwls single(std::vector<double> x, std::vector<double> y)
{
    const auto size = static_cast<BIYOS_offset>(x.size());
    return Pwls{.counts = {size}, .x = std::move(x), .y = std::move(y)};
}

void expect_refused(const Env &env, const Model &model, const Pwls &pwls, const std::string &text)
{
    const Model before(env, model);
    BIYOS_index pos = -1;
    EXPECT_EQ(add_pwls(model, pwls, &pos), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: " + text);
    EXPECT_EQ(pos, -1);
    EXPECT_EQ(biyos::difference(before.get()->core, model.get()->core), "");
}

Postfix written(const Model &model, BIYOS_index function)
{
    BIYOS_offset nnz = -1;
    BIYOS_offset nval = -1;
    EXPECT_EQ(BIYOS_getnexprsp(model.get(), 1, &function, &nnz, &nval), BIYOS_OK);
    EXPECT_EQ(nval, 0);
    Postfix found{
        .counts = {-1},
        .codes = std::vector<int>(static_cast<std::size_t>(nnz), -1),
        .arguments = std::vector<BIYOS_offset>(static_cast<std::size_t>(nnz), -1),
    };
    EXPECT_EQ(BIYOS_getexprsp(model.get(),
                              1,
                              &function,
                              nnz,
                              0,
                              found.counts.data(),
                              found.codes.data(),
                              found.arguments.data(),
                              nullptr),
              BIYOS_OK);
    return found;
}

void expect_postfix(const Postfix &found, const Postfix &wanted)
{
    EXPECT_EQ(found.counts, wanted.counts);
    EXPECT_EQ(found.codes, wanted.codes);
    EXPECT_EQ(found.arguments, wanted.arguments);
}

void attach(const Model &model, BIYOS_index function, BIYOS_offset node)
{
    EXPECT_EQ(BIYOS_attachexprsp(model.get(), 1, &function, &node), BIYOS_OK);
}

void add_functions(const Model &model, int n)
{
    ASSERT_EQ(BIYOS_addvar(model.get(), n, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model.get(), n, nullptr), BIYOS_OK);
}

}

TEST(PiecewiseTest, Read)
{
    const Env env;
    const Model model(env);
    BIYOS_index pos = -1;
    ASSERT_EQ(add_pwls(model, kSample, &pos), BIYOS_OK);
    EXPECT_EQ(pos, 0);
    ASSERT_EQ(add_pwls(model, kSample, &pos), BIYOS_OK);
    EXPECT_EQ(pos, 2);
    EXPECT_EQ(BIYOS_addpwl(model.get(), 0, nullptr, nullptr, nullptr, &pos), BIYOS_OK);
    EXPECT_EQ(pos, 4);
    EXPECT_EQ(n_pwls(model), 4);
    const Pwls twice{
        .counts = {3, 2, 3, 2},
        .x = {-1.0, 0.5, 2.0, -0.0, 0.1, -1.0, 0.5, 2.0, -0.0, 0.1},
        .y = {1.0, -0.0, 3.0, 0.3, 7.0, 1.0, -0.0, 3.0, 0.3, 7.0},
    };
    expect_pwls(read_all(model), twice);
    EXPECT_EQ(env.message(), "");
}

TEST(PiecewiseTest, Listed)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const Pwls wanted{
        .counts = {2, 3, 2},
        .x = {-0.0, 0.1, -1.0, 0.5, 2.0, -0.0, 0.1},
        .y = {0.3, 7.0, 1.0, -0.0, 3.0, 0.3, 7.0},
    };
    expect_pwls(read_listed(model, {1, 0, 1}), wanted);
    expect_pwls(read_listed(model, {0, 1}), read_all(model));
    expect_pwls(read_listed(model, {}), Pwls{});
    BIYOS_offset nnz = -1;
    EXPECT_EQ(BIYOS_getnpwlptrg(model.get(), 1, 1, &nnz), BIYOS_OK);
    EXPECT_EQ(nnz, 0);
    EXPECT_EQ(env.message(), "");
}

TEST(PiecewiseTest, Capacity)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Pwls read = sized(2, 6);
    const Pwls untouched = read;
    EXPECT_EQ(read_range(model, 0, 4, read), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': holds 4 of the 5 points");
    EXPECT_EQ(read_list(model, {0, 1}, 4, read), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': holds 4 of the 5 points");
    EXPECT_EQ(read_range(model, 0, -1, read), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cap': -1 is negative");
    expect_pwls(read, untouched);
    ASSERT_EQ(read_range(model, 0, 6, read), BIYOS_OK);
    EXPECT_EQ(read.counts, kSample.counts);
    EXPECT_EQ(bits_of(read.x).back(), std::bit_cast<std::uint64_t>(9.0));
    const BIYOS_index first = 0;
    std::vector<double> y(3, 9.0);
    EXPECT_EQ(BIYOS_getpwlsp(model.get(), 1, &first, 3, nullptr, nullptr, y.data()), BIYOS_OK);
    EXPECT_EQ(bits_of(y), bits_of({1.0, -0.0, 3.0}));
}

TEST(PiecewiseTest, Refusals)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    const double big = std::numeric_limits<double>::max();
    Pwls two = kSample;
    two.counts = {2, -1};
    expect_refused(env, model, two, "argument 'cnt': -1 is negative at entry 1");
    two.counts = {2, (std::numeric_limits<BIYOS_offset>::max)()};
    expect_refused(env, model, two, "argument 'cnt': sum overflows at entry 1");
    two.counts = {3, 1};
    expect_refused(env, model, two, "pwl 3: a pwl takes 2 points or more, not 1");
    expect_refused(env, model, single({0.0, 0.0}, {1.0, 2.0}), "pwl 2: x 1 is not above x 0");
    expect_refused(env, model, single({1.0, 0.0}, {1.0, 2.0}), "pwl 2: x 1 is not above x 0");
    expect_refused(env, model, single({0.0, kNan}, {1.0, 2.0}), "pwl 2: x 1 is NaN");
    expect_refused(env, model, single({0.0, 1.0}, {1.0, -kInf}), "pwl 2: y 1 is minus infinity");
    expect_refused(env,
                   model,
                   single({-big, big}, {0.0, 1.0}),
                   "pwl 2: the pwl's width from point 0 to point 1 is infinity");
    EXPECT_EQ(BIYOS_addpwl(model.get(), 1, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'cnt': is null");
    const BIYOS_offset count = 2;
    EXPECT_EQ(BIYOS_addpwl(model.get(), 1, &count, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'x': is null");
    const double points[] = {0.0, 1.0};
    EXPECT_EQ(BIYOS_addpwl(model.get(), 1, &count, points, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'y': is null");
    EXPECT_EQ(BIYOS_addpwl(model.get(), -1, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    expect_pwls(read_all(model), kSample);
}

TEST(PiecewiseTest, Unknown)
{
    const Env env;
    const Model model(env);
    add_sample(model);
    Pwls read = sized(2, 5);
    EXPECT_EQ(read_range(model, 1, 5, read), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: pwl 2: unknown");
    EXPECT_EQ(read_list(model, {1, 2}, 5, read), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: pwl 2: unknown");
    const BIYOS_index below = -1;
    BIYOS_offset nnz = 7;
    EXPECT_EQ(BIYOS_getnpwlptsp(model.get(), 1, &below, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: pwl -1: unknown");
    EXPECT_EQ(BIYOS_getnpwlptrg(model.get(), -1, 0, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: pwl -1: unknown");
    EXPECT_EQ(nnz, 7);
    EXPECT_EQ(BIYOS_getnpwlptrg(model.get(), 0, 1, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'nnz': is null");
    EXPECT_EQ(BIYOS_getnpwl(model.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': is null");
    expect_pwls(read, sized(2, 5));
}

TEST(PiecewiseTest, Node)
{
    const Env env;
    const Model model(env);
    add_functions(model, 2);
    add_sample(model);
    BIYOS_offset x = kNoNode;
    ASSERT_EQ(BIYOS_addvarnode(model.get(), 1, &x), BIYOS_OK);
    BIYOS_offset node = kNoNode;
    ASSERT_EQ(BIYOS_addpwlnode(model.get(), 1, x, &node), BIYOS_OK);
    EXPECT_EQ(node, x + 1);
    attach(model, 0, node);
    const BIYOS_offset count = 2;
    const int codes[] = {BIYOS_OP_VAR, BIYOS_OP_PWL};
    const BIYOS_offset arguments[] = {1, 1};
    BIYOS_offset top = kNoNode;
    ASSERT_EQ(BIYOS_addexpr(model.get(), 1, &count, codes, arguments, nullptr, &top), BIYOS_OK);
    attach(model, 1, top);
    const Postfix wanted{
        .counts = {2},
        .codes = {BIYOS_OP_VAR, BIYOS_OP_PWL},
        .arguments = {1, 1},
    };
    expect_postfix(written(model, 0), wanted);
    expect_postfix(written(model, 1), wanted);
    EXPECT_EQ(env.message(), "");
}

TEST(PiecewiseTest, NodeRefused)
{
    const Env env;
    const Model model(env);
    add_functions(model, 1);
    add_sample(model);
    BIYOS_offset x = kNoNode;
    ASSERT_EQ(BIYOS_addvarnode(model.get(), 0, &x), BIYOS_OK);
    const Model before(env, model);
    BIYOS_offset node = kNoNode;
    EXPECT_EQ(BIYOS_addpwlnode(model.get(), 2, x, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: expression: pwl 2 is unknown");
    EXPECT_EQ(BIYOS_addpwlnode(model.get(), -1, x, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: expression: pwl -1 is unknown");
    EXPECT_EQ(BIYOS_addpwlnode(model.get(), 0, x + 1, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: expression: node 1 is unknown");
    EXPECT_EQ(node, kNoNode);
    EXPECT_EQ(biyos::difference(before.get()->core, model.get()->core), "");
    EXPECT_EQ(BIYOS_addpwlnode(model.get(), 0, x, nullptr), BIYOS_OK);
}

TEST(PiecewiseTest, Copy)
{
    const Env env;
    const Model model(env);
    add_functions(model, 1);
    add_sample(model);
    BIYOS_offset x = kNoNode;
    ASSERT_EQ(BIYOS_addvarnode(model.get(), 0, &x), BIYOS_OK);
    BIYOS_offset node = kNoNode;
    ASSERT_EQ(BIYOS_addpwlnode(model.get(), 1, x, &node), BIYOS_OK);
    attach(model, 0, node);
    const Model copy(env, model);
    EXPECT_EQ(biyos::difference(copy.get()->core, model.get()->core), "");
    expect_pwls(read_all(copy), kSample);
    expect_postfix(written(copy, 0), written(model, 0));
    ASSERT_EQ(add_pwls(copy, kSample, nullptr), BIYOS_OK);
    EXPECT_EQ(n_pwls(model), 2);
    EXPECT_EQ(biyos::difference(copy.get()->core, model.get()->core), "pwl count 4 against 2");
}
