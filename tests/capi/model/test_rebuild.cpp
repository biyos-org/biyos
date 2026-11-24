#include "biyos/biyos.h"
#include "capi/entry.hpp"
#include "handles.hpp"
#include "model/compare/compare.hpp"
#include "sample.hpp"
#include "tolerance.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::test::below;
using biyos::test::build_sample;
using biyos::test::Engine;
using biyos::test::Env;
using biyos::test::Model;
using biyos::test::ToleranceCalls;

using Counter = BIYOS_status (*)(const BIYOS_model *, BIYOS_index *);
using Getter = decltype(&BIYOS_getvarnamerg);
using Setter = decltype(&BIYOS_setvarnamesp);

constexpr std::array<std::uint32_t, 8> kSeeds{1, 2, 3, 5, 8, 13, 21, 34};
constexpr std::array kWide{BIYOS_OP_SUM, BIYOS_OP_PROD, BIYOS_OP_MAX, BIYOS_OP_MIN};
constexpr int kDepth = 4;

struct Program
{
    std::vector<BIYOS_offset> starts{0};
    std::vector<BIYOS_int> codes;
    std::vector<BIYOS_offset> arguments;
    std::vector<BIYOS_real> values;
};

struct Writer
{
    Engine &engine;
    BIYOS_index variables;
    BIYOS_offset held;
    std::vector<BIYOS_offset> kept;
    Program program;
};

struct NameCalls
{
    Counter count;
    Getter get;
    Setter set;
};

const std::array kNameCalls{
    NameCalls{
        .count = BIYOS_getnvar,
        .get = BIYOS_getvarnamerg,
        .set = BIYOS_setvarnamesp,
    },
    NameCalls{
        .count = BIYOS_getnfun,
        .get = BIYOS_getfunnamerg,
        .set = BIYOS_setfunnamesp,
    },
    NameCalls{
        .count = BIYOS_getncon,
        .get = BIYOS_getconnamerg,
        .set = BIYOS_setconnamesp,
    },
    NameCalls{
        .count = BIYOS_getnobj,
        .get = BIYOS_getobjnamerg,
        .set = BIYOS_setobjnamesp,
    },
    NameCalls{
        .count = BIYOS_getnsos,
        .get = BIYOS_getsosnamerg,
        .set = BIYOS_setsosnamesp,
    },
    NameCalls{
        .count = BIYOS_getncone,
        .get = BIYOS_getconenamerg,
        .set = BIYOS_setconenamesp,
    },
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

std::size_t at(BIYOS_offset n)
{
    return static_cast<std::size_t>(n);
}

BIYOS_index count_of(const Model &model, Counter count)
{
    BIYOS_index n = -1;
    EXPECT_EQ(count(model.get(), &n), BIYOS_OK);
    return n;
}

BIYOS_offset graph_size(const Model &model)
{
    return model.get()->core.graph().size();
}

double value(Engine &engine)
{
    if (below(engine, 10) == 0)
    {
        return -0.0;
    }
    return (below(engine, 9) - 4) / 2.0;
}

void put(Writer &writer, int code, BIYOS_offset argument)
{
    writer.program.codes.push_back(code);
    writer.program.arguments.push_back(argument);
}

void put_value(Writer &writer, double value)
{
    writer.program.values.push_back(value);
}

void put_leaf(Writer &writer)
{
    Engine &engine = writer.engine;
    const int choice = below(engine, 8);
    if (choice == 0)
    {
        put(writer, BIYOS_OP_CONST, 0);
        put_value(writer, value(engine));
        return;
    }
    if (choice == 1 && writer.held > 0)
    {
        put(writer, BIYOS_OP_NODE, below(engine, static_cast<int>(writer.held)));
        return;
    }
    if (choice == 2 && !writer.kept.empty())
    {
        const auto n = static_cast<int>(writer.kept.size());
        put(writer, BIYOS_OP_REF, writer.kept[at(below(engine, n))]);
        return;
    }
    put(writer, BIYOS_OP_VAR, below(engine, writer.variables));
}

int random_code(Engine &engine)
{
    if (below(engine, 3) > 0)
    {
        return below(engine, 2) == 0 ? BIYOS_OP_SUM : BIYOS_OP_PROD;
    }
    return BIYOS_OP_NEG + below(engine, BIYOS_OP_ERF - BIYOS_OP_NEG + 1);
}

int arity(Engine &engine, int code)
{
    if (code == BIYOS_OP_DIV || code == BIYOS_OP_POW)
    {
        return 2;
    }
    for (const int wide : kWide)
    {
        if (code == wide)
        {
            return 1 + below(engine, 3);
        }
    }
    return 1;
}

void put_values(Writer &writer, int code, int n)
{
    Engine &engine = writer.engine;
    if (code == BIYOS_OP_SUM)
    {
        for (int k = 0; k <= n; ++k)
        {
            put_value(writer, value(engine));
        }
    }
    if (code == BIYOS_OP_PROD)
    {
        put_value(writer, value(engine));
    }
    if (code == BIYOS_OP_CPOW)
    {
        put_value(writer, below(engine, 2) == 0 ? 2.0 : value(engine));
    }
    if (code == BIYOS_OP_SPOW)
    {
        put_value(writer, (1 + below(engine, 8)) / 4.0);
    }
}

void put_keep(Writer &writer)
{
    writer.kept.push_back(static_cast<BIYOS_offset>(writer.program.codes.size()));
    put(writer, BIYOS_OP_KEEP, 1);
}

void put_tree(Writer &writer, int depth)
{
    if (depth == 0 || below(writer.engine, 4) == 0)
    {
        put_leaf(writer);
        return;
    }
    const int code = random_code(writer.engine);
    const int n = arity(writer.engine, code);
    for (int k = 0; k < n; ++k)
    {
        put_tree(writer, depth - 1);
    }
    put(writer, code, n);
    put_values(writer, code, n);
    if (below(writer.engine, 4) == 0)
    {
        put_keep(writer);
    }
}

Program random_program(const Model &model, Engine &engine, int n)
{
    Writer writer{
        .engine = engine,
        .variables = count_of(model, BIYOS_getnvar),
        .held = graph_size(model),
        .kept = {},
        .program = {},
    };
    for (int k = 0; k < n; ++k)
    {
        put_tree(writer, kDepth);
        writer.program.starts.push_back(static_cast<BIYOS_offset>(writer.program.codes.size()));
    }
    return std::move(writer.program);
}

std::vector<BIYOS_offset> add_program(const Model &model, const Program &program)
{
    const auto n = static_cast<int>(program.starts.size()) - 1;
    std::vector<BIYOS_offset> nodes(at(n), -1);
    const BIYOS_status status = BIYOS_addexpr(model.get(),
                                              n,
                                              program.starts.data(),
                                              program.codes.data(),
                                              program.arguments.data(),
                                              program.values.data(),
                                              nodes.data());
    EXPECT_EQ(status, BIYOS_OK);
    return nodes;
}

std::vector<BIYOS_offset> add_random(const Model &model, Engine &engine, int n)
{
    const int first = 1 + below(engine, n);
    std::vector<BIYOS_offset> tops = add_program(model, random_program(model, engine, first));
    if (first < n)
    {
        const std::vector<BIYOS_offset> more =
            add_program(model, random_program(model, engine, n - first));
        tops.insert(tops.end(), more.begin(), more.end());
    }
    return tops;
}

std::vector<BIYOS_index> shuffled(Engine &engine, BIYOS_index n)
{
    std::vector<BIYOS_index> order;
    for (BIYOS_index k = 0; k < n; ++k)
    {
        order.push_back(k);
        std::swap(order.back(), order[at(below(engine, k + 1))]);
    }
    return order;
}

void attach(const Model &model,
            const std::vector<BIYOS_index> &functions,
            const std::vector<BIYOS_offset> &nodes)
{
    const auto n = static_cast<BIYOS_index>(functions.size());
    ASSERT_EQ(BIYOS_attachexprsp(model.get(), n, functions.data(), nodes.data()), BIYOS_OK);
}

BIYOS_offset pick(Engine &engine, const std::vector<BIYOS_offset> &tops)
{
    return tops[at(below(engine, static_cast<int>(tops.size())))];
}

void attach_all(const Model &model, Engine &engine, const std::vector<BIYOS_offset> &tops)
{
    const std::vector<BIYOS_index> order = shuffled(engine, count_of(model, BIYOS_getnfun));
    std::vector<BIYOS_index> functions;
    std::vector<BIYOS_offset> nodes;
    for (std::size_t k = 0; k < order.size(); ++k)
    {
        if (k < tops.size() || below(engine, 2) == 0)
        {
            functions.push_back(order[k]);
            nodes.push_back(k < tops.size() ? tops[k] : pick(engine, tops));
        }
    }
    attach(model, functions, nodes);
}

void attach_again(const Model &model, Engine &engine, const std::vector<BIYOS_offset> &tops)
{
    const BIYOS_index n = count_of(model, BIYOS_getnfun);
    std::vector<BIYOS_index> functions;
    std::vector<BIYOS_offset> nodes;
    for (BIYOS_index k = 0; k < n; ++k)
    {
        if (below(engine, 3) == 0)
        {
            functions.push_back(k);
            nodes.push_back(pick(engine, tops));
        }
    }
    attach(model, functions, nodes);
}

void copy_variables(const Model &from, const Model &to)
{
    const BIYOS_index n = count_of(from, BIYOS_getnvar);
    std::vector<BIYOS_int> kinds(at(n));
    std::vector<BIYOS_real> lower(at(n));
    std::vector<BIYOS_real> upper(at(n));
    const BIYOS_status read =
        BIYOS_getvarrg(from.get(), 0, n, kinds.data(), lower.data(), upper.data());
    ASSERT_EQ(read, BIYOS_OK);
    const BIYOS_status added =
        BIYOS_addvar(to.get(), n, kinds.data(), lower.data(), upper.data(), nullptr);
    ASSERT_EQ(added, BIYOS_OK);
}

void copy_functions(const Model &from, const Model &to)
{
    const BIYOS_index n = count_of(from, BIYOS_getnfun);
    std::vector<BIYOS_real> constants(at(n));
    ASSERT_EQ(BIYOS_getcstrg(from.get(), 0, n, constants.data()), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(to.get(), n, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcstrg(to.get(), 0, n, constants.data()), BIYOS_OK);
}

void copy_linear(const Model &from, const Model &to)
{
    const BIYOS_index n = count_of(from, BIYOS_getnfun);
    BIYOS_offset nnz = -1;
    ASSERT_EQ(BIYOS_getnrlintermrg(from.get(), 0, n, &nnz), BIYOS_OK);
    std::vector<BIYOS_offset> starts(at(n) + 1);
    std::vector<BIYOS_index> variables(at(nnz));
    std::vector<BIYOS_real> values(at(nnz));
    const BIYOS_status read =
        BIYOS_getrlintermrg(from.get(), 0, n, nnz, starts.data(), variables.data(), values.data());
    ASSERT_EQ(read, BIYOS_OK);
    const BIYOS_status added =
        BIYOS_addrlintermrg(to.get(), 0, n, starts.data(), variables.data(), values.data());
    ASSERT_EQ(added, BIYOS_OK);
}

void copy_pairs(const Model &from, const Model &to)
{
    const BIYOS_index n = count_of(from, BIYOS_getnfun);
    BIYOS_offset nnz = -1;
    ASSERT_EQ(BIYOS_getnquadtermrg(from.get(), 0, n, &nnz), BIYOS_OK);
    std::vector<BIYOS_offset> starts(at(n) + 1);
    std::vector<BIYOS_index> rows(at(nnz));
    std::vector<BIYOS_index> columns(at(nnz));
    std::vector<BIYOS_real> values(at(nnz));
    const BIYOS_status read = BIYOS_getquadtermrg(from.get(),
                                                  0,
                                                  n,
                                                  nnz,
                                                  starts.data(),
                                                  rows.data(),
                                                  columns.data(),
                                                  values.data());
    ASSERT_EQ(read, BIYOS_OK);
    const BIYOS_status added = BIYOS_addquadtermrg(to.get(),
                                                   0,
                                                   n,
                                                   starts.data(),
                                                   rows.data(),
                                                   columns.data(),
                                                   values.data());
    ASSERT_EQ(added, BIYOS_OK);
}

Program read_expressions(const Model &model)
{
    const BIYOS_index n = count_of(model, BIYOS_getnfun);
    BIYOS_offset nnz = -1;
    BIYOS_offset nval = -1;
    EXPECT_EQ(BIYOS_getnexprrg(model.get(), 0, n, &nnz, &nval), BIYOS_OK);
    Program read{
        .starts = std::vector<BIYOS_offset>(at(n) + 1),
        .codes = std::vector<BIYOS_int>(at(nnz)),
        .arguments = std::vector<BIYOS_offset>(at(nnz)),
        .values = std::vector<BIYOS_real>(at(nval)),
    };
    const BIYOS_status status = BIYOS_getexprrg(model.get(),
                                                0,
                                                n,
                                                nnz,
                                                nval,
                                                read.starts.data(),
                                                read.codes.data(),
                                                read.arguments.data(),
                                                read.values.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

void copy_expressions(const Model &from, const Model &to)
{
    const std::vector<BIYOS_offset> nodes = add_program(to, read_expressions(from));
    const auto n = static_cast<BIYOS_index>(nodes.size());
    ASSERT_EQ(BIYOS_attachexprrg(to.get(), 0, n, nodes.data()), BIYOS_OK);
}

void copy_constraints(const Model &from, const Model &to)
{
    const BIYOS_index n = count_of(from, BIYOS_getncon);
    std::vector<BIYOS_index> functions(at(n));
    std::vector<BIYOS_real> lower(at(n));
    std::vector<BIYOS_real> upper(at(n));
    const BIYOS_status read =
        BIYOS_getconrg(from.get(), 0, n, functions.data(), lower.data(), upper.data());
    ASSERT_EQ(read, BIYOS_OK);
    const BIYOS_status added =
        BIYOS_addcon(to.get(), n, functions.data(), lower.data(), upper.data(), nullptr);
    ASSERT_EQ(added, BIYOS_OK);
}

void copy_objectives(const Model &from, const Model &to)
{
    const BIYOS_index n = count_of(from, BIYOS_getnobj);
    std::vector<BIYOS_index> functions(at(n));
    std::vector<BIYOS_int> senses(at(n));
    std::vector<BIYOS_int> priorities(at(n));
    std::vector<BIYOS_real> weights(at(n));
    const BIYOS_status read = BIYOS_getobjrg(from.get(),
                                             0,
                                             n,
                                             functions.data(),
                                             senses.data(),
                                             priorities.data(),
                                             weights.data());
    ASSERT_EQ(read, BIYOS_OK);
    const BIYOS_status added = BIYOS_addobj(to.get(),
                                            n,
                                            functions.data(),
                                            senses.data(),
                                            priorities.data(),
                                            weights.data(),
                                            nullptr);
    ASSERT_EQ(added, BIYOS_OK);
}

void copy_sos(const Model &from, const Model &to)
{
    const BIYOS_index n = count_of(from, BIYOS_getnsos);
    BIYOS_offset nnz = -1;
    ASSERT_EQ(BIYOS_getnsosvarrg(from.get(), 0, n, &nnz), BIYOS_OK);
    std::vector<BIYOS_int> kinds(at(n));
    std::vector<BIYOS_offset> starts(at(n) + 1);
    std::vector<BIYOS_index> variables(at(nnz));
    std::vector<BIYOS_real> weights(at(nnz));
    const BIYOS_status read = BIYOS_getsosrg(from.get(),
                                             0,
                                             n,
                                             nnz,
                                             kinds.data(),
                                             starts.data(),
                                             variables.data(),
                                             weights.data());
    ASSERT_EQ(read, BIYOS_OK);
    const BIYOS_status added = BIYOS_addsos(to.get(),
                                            n,
                                            kinds.data(),
                                            starts.data(),
                                            variables.data(),
                                            weights.data(),
                                            nullptr);
    ASSERT_EQ(added, BIYOS_OK);
}

void copy_cones(const Model &from, const Model &to)
{
    const BIYOS_index n = count_of(from, BIYOS_getncone);
    BIYOS_offset nnz = -1;
    BIYOS_offset nprm = -1;
    ASSERT_EQ(BIYOS_getnconefunrg(from.get(), 0, n, &nnz), BIYOS_OK);
    ASSERT_EQ(BIYOS_getnconeprmrg(from.get(), 0, n, &nprm), BIYOS_OK);
    std::vector<BIYOS_int> kinds(at(n));
    std::vector<BIYOS_offset> starts(at(n) + 1);
    std::vector<BIYOS_index> functions(at(nnz));
    std::vector<BIYOS_offset> parameter_starts(at(n) + 1);
    std::vector<BIYOS_real> parameters(at(nprm));
    const BIYOS_status members =
        BIYOS_getconerg(from.get(), 0, n, nnz, kinds.data(), starts.data(), functions.data());
    ASSERT_EQ(members, BIYOS_OK);
    const BIYOS_status read =
        BIYOS_getconeprmrg(from.get(), 0, n, nprm, parameter_starts.data(), parameters.data());
    ASSERT_EQ(read, BIYOS_OK);
    const BIYOS_status added = BIYOS_addcone(to.get(),
                                             n,
                                             kinds.data(),
                                             starts.data(),
                                             functions.data(),
                                             parameter_starts.data(),
                                             parameters.data(),
                                             nullptr);
    ASSERT_EQ(added, BIYOS_OK);
}

void copy_tolerances(const Model &from, const Model &to)
{
    const BIYOS_index n = count_of(from, BIYOS_getnobj);
    std::vector<BIYOS_real> absolute(at(n));
    std::vector<BIYOS_real> relative(at(n));
    ASSERT_EQ(BIYOS_getobjtolrg(from.get(), 0, n, absolute.data(), relative.data()), BIYOS_OK);
    ASSERT_EQ(BIYOS_setobjtolrg(to.get(), 0, n, absolute.data(), relative.data()), BIYOS_OK);
}

struct Known
{
    std::vector<BIYOS_index> indices;
    std::vector<BIYOS_real> values;
};

Known known_of(const std::vector<BIYOS_real> &values)
{
    Known known;
    for (std::size_t k = 0; k < values.size(); ++k)
    {
        if (!std::isnan(values[k]))
        {
            known.indices.push_back(static_cast<BIYOS_index>(k));
            known.values.push_back(values[k]);
        }
    }
    return known;
}

void set_known(const Model &to,
               const ToleranceCalls &calls,
               const Known &absolute,
               const Known &relative)
{
    BIYOS_model *model = to.get();
    const auto n_absolute = static_cast<BIYOS_index>(absolute.indices.size());
    const auto n_relative = static_cast<BIYOS_index>(relative.indices.size());
    const BIYOS_status first = calls.set_listed(model,
                                                n_absolute,
                                                absolute.indices.data(),
                                                absolute.values.data(),
                                                nullptr);
    ASSERT_EQ(first, BIYOS_OK);
    const BIYOS_status second = calls.set_listed(model,
                                                 n_relative,
                                                 relative.indices.data(),
                                                 nullptr,
                                                 relative.values.data());
    ASSERT_EQ(second, BIYOS_OK);
}

void copy_family(const Model &from, const Model &to, const ToleranceCalls &calls)
{
    const BIYOS_index n = count_of(from, calls.count);
    std::vector<BIYOS_real> absolute(at(n));
    std::vector<BIYOS_real> relative(at(n));
    ASSERT_EQ(calls.get_ranged(from.get(), 0, n, absolute.data(), relative.data()), BIYOS_OK);
    set_known(to, calls, known_of(absolute), known_of(relative));
}

void copy_names(const Model &from, const Model &to, const NameCalls &calls)
{
    const BIYOS_index n = count_of(from, calls.count);
    std::vector<const char *> names(at(n));
    ASSERT_EQ(calls.get(from.get(), 0, n, names.data()), BIYOS_OK);
    std::vector<BIYOS_index> named;
    std::vector<const char *> given;
    for (BIYOS_index k = 0; k < n; ++k)
    {
        if (names[at(k)][0] != '\0')
        {
            named.push_back(k);
            given.push_back(names[at(k)]);
        }
    }
    const auto size = static_cast<BIYOS_index>(named.size());
    ASSERT_EQ(calls.set(to.get(), size, named.data(), given.data()), BIYOS_OK);
}

void copy_title(const Model &from, const Model &to)
{
    const char *name = nullptr;
    ASSERT_EQ(BIYOS_getmdlname(from.get(), &name), BIYOS_OK);
    if (name[0] != '\0')
    {
        ASSERT_EQ(BIYOS_setmdlname(to.get(), name), BIYOS_OK);
    }
}

void copy_model(const Model &from, const Model &to)
{
    copy_variables(from, to);
    copy_functions(from, to);
    copy_linear(from, to);
    copy_pairs(from, to);
    copy_expressions(from, to);
    copy_constraints(from, to);
    copy_objectives(from, to);
    copy_tolerances(from, to);
    copy_sos(from, to);
    copy_cones(from, to);
    copy_family(from, to, biyos::test::kVariableTolerances);
    copy_family(from, to, biyos::test::kConstraintTolerances);
    copy_family(from, to, biyos::test::kConeTolerances);
    copy_title(from, to);
    for (const NameCalls &calls : kNameCalls)
    {
        copy_names(from, to, calls);
    }
}

void build_random(const Model &model, Engine &engine)
{
    build_sample(model.get(), engine);
    const BIYOS_index functions = count_of(model, BIYOS_getnfun);
    const std::vector<BIYOS_offset> tops = add_random(model, engine, 1 + below(engine, functions));
    attach_all(model, engine, tops);
    attach_again(model, engine, tops);
    if (below(engine, 2) == 0)
    {
        const std::string name = "m" + std::to_string(below(engine, 8));
        ASSERT_EQ(BIYOS_setmdlname(model.get(), name.c_str()), BIYOS_OK);
    }
}

std::string difference(const Model &left, const Model &right)
{
    return biyos::difference(left.get()->core, right.get()->core);
}

void expect_same(const Program &found, const Program &wanted)
{
    EXPECT_EQ(found.starts, wanted.starts);
    EXPECT_EQ(found.codes, wanted.codes);
    EXPECT_EQ(found.arguments, wanted.arguments);
    EXPECT_EQ(bits_of(found.values), bits_of(wanted.values));
}

TEST(RebuildTest, Random)
{
    for (const std::uint32_t seed : kSeeds)
    {
        SCOPED_TRACE(seed);
        const Env env;
        const Model model(env);
        const Model rebuilt(env);
        Engine engine(seed);
        build_random(model, engine);
        copy_model(model, rebuilt);
        EXPECT_EQ(difference(model, rebuilt), "");
        expect_same(read_expressions(rebuilt), read_expressions(model));
        EXPECT_EQ(env.message(), "");
    }
}

}
