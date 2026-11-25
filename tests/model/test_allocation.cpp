#include "allocations.hpp"
#include "biyos/biyos.h"
#include "core/error.hpp"
#include "core/index.hpp"
#include "expr/graph/graph.hpp"
#include "model/compare/compare.hpp"
#include "model/expressions/postfix.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using biyos::Complement;
using biyos::ConeKind;
using biyos::Family;
using biyos::Graph;
using biyos::Index;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Offset;
using biyos::Variable;
using biyos::VariableKind;

constexpr std::size_t kAttempts = 100;
constexpr std::string_view kLong = "a name too long to sit inside the string itself";

struct PendingFailure
{
    explicit PendingFailure(std::size_t successes) noexcept { biyos::test::fail_after(successes); }
    PendingFailure(const PendingFailure &) = delete;
    PendingFailure &operator=(const PendingFailure &) = delete;
    ~PendingFailure() { biyos::test::stop_failing(); }
};

std::vector<std::string> numbered(Index n)
{
    std::vector<std::string> texts;
    for (Index k = 0; k < n; ++k)
    {
        texts.push_back("v" + std::to_string(k));
    }
    return texts;
}

Model named(Index n)
{
    Model model;
    const auto size = static_cast<std::size_t>(n) + 2;
    model.add_variables(std::vector<Variable>(size, biyos::widest(VariableKind::Continuous)));
    std::vector<Index> indices;
    for (Index k = 0; k < n; ++k)
    {
        indices.push_back(k);
    }
    const std::vector<std::string> texts = numbered(n);
    model.set_names(Family::Variable,
                    indices,
                    std::vector<std::string_view>(texts.begin(), texts.end()));
    return model;
}

void rename(Model &model, Index n)
{
    const std::vector<Index> indices{0, 1, 2, n, n, n + 1};
    const std::vector<std::string_view> names{"v1", "v0", kLong, "e", "f", "g"};
    model.set_names(Family::Variable, indices, names);
}

bool runs_out(Model &model, Index n, std::size_t successes)
{
    const PendingFailure failure(successes);
    try
    {
        rename(model, n);
    }
    catch (const std::bad_alloc &)
    {
        return true;
    }
    return false;
}

std::vector<std::string> names_of(const Model &model)
{
    std::vector<std::string> found;
    for (Index k = 0; k < model.n_variables(); ++k)
    {
        const std::string *name = model.name_of(Family::Variable, k);
        found.push_back(name == nullptr ? "none" : *name);
    }
    return found;
}

std::optional<Index> lookup(const Model &model, const std::string &name)
{
    try
    {
        return model.index_of(Family::Variable, name);
    }
    catch (const biyos::Error &)
    {
        return std::nullopt;
    }
}

std::vector<std::optional<Index>> lookups(const Model &model, Index n)
{
    std::vector<std::string> names = numbered(n);
    names.insert(names.end(), {"e", "f", "g", std::string(kLong)});
    std::vector<std::optional<Index>> found;
    for (const std::string &name : names)
    {
        found.push_back(lookup(model, name));
    }
    return found;
}

void expect_kept(Index n)
{
    Model wanted = named(n);
    rename(wanted, n);
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        Model model = named(n);
        const Model before = model;
        const std::string *kept = model.name_of(Family::Variable, n - 1);
        if (!runs_out(model, n, successes))
        {
            EXPECT_GT(successes, 0U);
            EXPECT_EQ(names_of(model), names_of(wanted));
            EXPECT_EQ(lookups(model, n), lookups(wanted, n));
            return;
        }
        EXPECT_EQ(names_of(model), names_of(before));
        EXPECT_EQ(lookups(model, n), lookups(before, n));
        EXPECT_EQ(model.name_of(Family::Variable, n - 1), kept);
        rename(model, n);
        EXPECT_EQ(names_of(model), names_of(wanted));
    }
    ADD_FAILURE() << "the call never completed";
}

Model with_terms()
{
    Model model;
    model.add_variables(std::vector<Variable>(4, biyos::widest(VariableKind::Continuous)));
    const std::vector<double> constants(3, 0.0);
    const std::vector<Offset> starts{0, 2, 2, 3};
    model.add_functions(constants, starts, {{1, 1.0}, {3, 2.0}, {2, 3.0}});
    return model;
}

bool runs_out_adding(Model &model, std::size_t successes)
{
    const std::vector<Index> functions{2, 0};
    const std::vector<Offset> starts{0, 2, 4};
    std::vector<LinearTerm> terms{{0, 1.0}, {2, 1.0}, {0, 1.0}, {2, 1.0}};
    const PendingFailure failure(successes);
    try
    {
        model.add_linear_terms(functions, starts, std::move(terms));
    }
    catch (const std::bad_alloc &)
    {
        return true;
    }
    return false;
}

Model with_nodes()
{
    Model model;
    model.add_variables(std::vector<Variable>(4, biyos::widest(VariableKind::Continuous)));
    const std::vector<Offset> x{model.add_variable_node(0)};
    model.add_node(BIYOS_OP_EXP, x, {});
    model.add_pwl(std::vector<double>{0.0, 1.0}, std::vector<double>{2.0, -1.0});
    return model;
}

void add_variable(Model &model)
{
    model.add_variable_node(3);
}

void add_points(Model &model)
{
    model.add_pwl(std::vector<double>{-1.0, 0.0, 2.0}, std::vector<double>{1.0, 0.0, 3.0});
}

void add_batch(Model &model)
{
    const std::vector<Offset> starts{0, 2, 5};
    const std::vector<double> x{0.0, 1.0, -1.0, 0.0, 2.0};
    const std::vector<double> y{2.0, -1.0, 1.0, 0.0, 3.0};
    model.add_piecewise_linears(starts, x, y);
}

void add_piecewise(Model &model)
{
    model.add_piecewise(0, 1);
}

void add_two(Model &model)
{
    const std::vector<double> constants{1.0, 2.0};
    const std::vector<Offset> starts{0, 1, 1};
    model.add_functions(constants, starts, {{0, 1.0}});
}

Model with_cone()
{
    Model model;
    const std::vector<double> constants(3, 0.0);
    const std::vector<Offset> starts(4, 0);
    model.add_functions(constants, starts, {});
    const ConeKind kind = ConeKind::Nonnegative;
    const Index function = 1;
    const std::vector<Offset> none{0, 0};
    model.add_cones(std::span(&kind, 1),
                    std::vector<Offset>{0, 1},
                    std::span(&function, 1),
                    none,
                    {});
    return model;
}

void add_cones(Model &model)
{
    const std::vector<ConeKind> kinds{ConeKind::SecondOrder, ConeKind::Power};
    const std::vector<Offset> starts{0, 2, 5};
    const std::vector<Index> functions{2, 0, 0, 1, 2};
    const std::vector<Offset> parameter_starts{0, 0, 2};
    const std::vector<double> parameters{0.25, 0.75};
    model.add_cones(kinds, starts, functions, parameter_starts, parameters);
}

void add_sum(Model &model)
{
    const std::vector<Offset> children{0, 1};
    const std::vector<double> values{0.0, 1.0, 2.0};
    model.add_node(BIYOS_OP_SUM, children, values);
}

Model with_roots()
{
    Model model = with_nodes();
    model.add_functions(std::vector<double>(3, 1.0),
                        std::vector<Offset>{0, 1, 1, 2},
                        {{2, 1.0}, {0, 3.0}});
    const std::vector<Index> held{2};
    const std::vector<Offset> one{0, 1};
    model.add_quadratic_terms(held, one, {{1, 1, 2.0}});
    const std::vector<Offset> x{0};
    const std::vector<double> values{0.5, 2.0};
    const std::vector<Offset> sum{model.add_node(BIYOS_OP_SUM, x, values)};
    const std::vector<Offset> pair{0, 0};
    const Offset square = model.add_node(BIYOS_OP_PROD, pair, std::vector<double>{3.0});
    const std::vector<Offset> terms{1, square, 0};
    model.add_node(BIYOS_OP_SUM, terms, std::vector<double>{1.0, 1.0, 1.0, 4.0});
    const std::vector<Index> functions{0, 1};
    const std::vector<Offset> roots{sum.front(), 1};
    model.attach_expressions(functions, roots);
    return model;
}

void attach_all(Model &model)
{
    const std::vector<Index> functions{2, 0, 1};
    const std::vector<Offset> nodes{4, 4, 1};
    model.attach_expressions(functions, nodes);
}

void decode_two(Model &model)
{
    const std::vector<Offset> starts{0, 6, 9};
    const std::vector<int> codes{
        BIYOS_OP_VAR,
        BIYOS_OP_VAR,
        BIYOS_OP_SUM,
        BIYOS_OP_NODE,
        BIYOS_OP_PROD,
        BIYOS_OP_SIN,
        BIYOS_OP_VAR,
        BIYOS_OP_CONST,
        BIYOS_OP_DIV,
    };
    const std::vector<Offset> arguments{2, 3, 2, 1, 2, 1, 1, 0, 2};
    const std::vector<double> values{0.0, 1.0, 2.0, 3.0, 4.0};
    model.add_expressions(
        biyos::Postfix{.starts = starts, .codes = codes, .arguments = arguments, .values = values});
}

void clear_two(Model &model)
{
    const std::vector<Index> functions{2, 0, 2};
    model.clear_functions(functions);
}

std::vector<Offset> roots_of(const Model &model)
{
    const std::span<const Offset> roots = model.roots(0, model.n_functions());
    return {roots.begin(), roots.end()};
}

bool runs_out_with(Model &model, void (*change)(Model &), std::size_t successes)
{
    const PendingFailure failure(successes);
    try
    {
        change(model);
    }
    catch (const std::bad_alloc &)
    {
        return true;
    }
    return false;
}

void expect_graph(void (*change)(Model &), Offset added, Offset pwls = 0)
{
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        Model model = with_nodes();
        const Graph::Mark before = model.graph().mark();
        if (!runs_out_with(model, change, successes))
        {
            EXPECT_GT(successes, 0U);
            EXPECT_EQ(model.graph().size(), before.nodes + added);
            EXPECT_EQ(model.graph().points().size(), before.pwls + pwls);
            return;
        }
        EXPECT_EQ(model.graph().mark(), before);
        change(model);
        Model clean = with_nodes();
        change(clean);
        EXPECT_EQ(biyos::difference(model, clean), "");
    }
    ADD_FAILURE() << "the call never completed";
}

std::size_t lookup_cost(const Model &model)
{
    const std::size_t start = biyos::test::allocations();
    EXPECT_EQ(model.index_of(Family::Variable, kLong), 2);
    EXPECT_NE(model.name_of(Family::Variable, 2), nullptr);
    return biyos::test::allocations() - start;
}

std::size_t copy_cost(const Model &model)
{
    const std::size_t start = biyos::test::allocations();
    const Model copy = model;
    return biyos::test::allocations() - start;
}

Model with_complements()
{
    Model model;
    model.add_functions(std::vector<double>{0.0}, std::vector<Offset>{0, 0}, {});
    const Complement held{.function = 0, .lower = 0.0, .upper = 1.0, .multiplier = 0};
    model.add_complements(std::vector<Complement>(4, held));
    return model;
}

void set_tolerances(Model &model)
{
    const std::vector<Index> indices{3, 0, 2};
    const std::vector<double> absolute{1e-6, 1e-7, 1e-8};
    model.set_tolerances(Family::Complement, indices, absolute, {});
}

void delete_two(Model &model)
{
    const std::array<Index, 2> indices{3, 0};
    model.delete_complements(indices);
}

}

TEST(AllocationTest, Renaming)
{
    for (Index n = 4; n <= 48; ++n)
    {
        SCOPED_TRACE(n);
        expect_kept(n);
    }
}

TEST(AllocationTest, NameLookup)
{
    Model model = named(4);
    rename(model, 4);
    EXPECT_EQ(lookup_cost(model), 0U);
}

TEST(AllocationTest, NameRelease)
{
    Model model = named(1);
    const std::size_t cost = copy_cost(model);
    const std::vector<Index> indices{1};
    const std::vector<std::string_view> names{kLong};
    model.set_names(Family::Variable, indices, names);
    model.delete_names(Family::Variable, indices);
    EXPECT_EQ(copy_cost(model), cost);
}

TEST(AllocationTest, LinearTerms)
{
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        Model model = with_terms();
        if (!runs_out_adding(model, successes))
        {
            EXPECT_GT(successes, 0U);
            EXPECT_EQ(model.linear_terms(0, 3).size(), 6U);
            return;
        }
        EXPECT_EQ(biyos::difference(model, with_terms()), "");
    }
    ADD_FAILURE() << "the call never completed";
}

TEST(AllocationTest, Functions)
{
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        const Model before = with_nodes();
        Model model = before;
        if (!runs_out_with(model, add_two, successes))
        {
            EXPECT_GT(successes, 0U);
            EXPECT_EQ(model.roots(0, 2).size(), 2U);
            EXPECT_EQ(model.roots(0, 2)[1], biyos::kNoRoot);
            return;
        }
        EXPECT_EQ(biyos::difference(model, before), "");
    }
    ADD_FAILURE() << "the call never completed";
}

TEST(AllocationTest, Cones)
{
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        const Model before = with_cone();
        Model model = before;
        if (!runs_out_with(model, add_cones, successes))
        {
            EXPECT_GT(successes, 0U);
            EXPECT_EQ(model.n_cones(), 3);
            EXPECT_EQ(model.cone_functions(0, 3).size(), 6U);
            EXPECT_EQ(model.cone_parameters(0, 3).size(), 2U);
            return;
        }
        EXPECT_EQ(biyos::difference(model, before), "");
    }
    ADD_FAILURE() << "the call never completed";
}

TEST(AllocationTest, VariableNode)
{
    expect_graph(add_variable, 1);
}

TEST(AllocationTest, SumNode)
{
    expect_graph(add_sum, 1);
}

TEST(AllocationTest, Points)
{
    expect_graph(add_points, 0, 1);
}

TEST(AllocationTest, PointBatch)
{
    expect_graph(add_batch, 0, 2);
}

TEST(AllocationTest, PiecewiseNode)
{
    expect_graph(add_piecewise, 1);
}

TEST(AllocationTest, Attach)
{
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        const Model before = with_roots();
        Model model = before;
        if (!runs_out_with(model, attach_all, successes))
        {
            EXPECT_GT(successes, 0U);
            EXPECT_EQ(model.linear_terms(0, 3).size(), 3U);
            EXPECT_EQ(model.quadratic_terms(0, 3).size(), 3U);
            EXPECT_EQ(roots_of(model), (std::vector<Offset>{1, 5, 1}));
            return;
        }
        EXPECT_EQ(biyos::difference(model, before), "");
        EXPECT_EQ(model.graph().mark(), before.graph().mark());
        EXPECT_EQ(roots_of(model), roots_of(before));
    }
    ADD_FAILURE() << "the call never completed";
}

TEST(AllocationTest, Clear)
{
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        Model model = with_roots();
        const Model before = model;
        if (!runs_out_with(model, clear_two, successes))
        {
            EXPECT_GT(successes, 0U);
            EXPECT_EQ(model.linear_terms(0, 3).size(), 0U);
            EXPECT_EQ(model.quadratic_terms(0, 3).size(), 0U);
            EXPECT_EQ(roots_of(model), (std::vector<Offset>{biyos::kNoRoot, 1, biyos::kNoRoot}));
            return;
        }
        EXPECT_EQ(biyos::difference(model, before), "");
        EXPECT_EQ(roots_of(model), roots_of(before));
    }
    ADD_FAILURE() << "the call never completed";
}

TEST(AllocationTest, Postfix)
{
    expect_graph(decode_two, 8);
}

TEST(AllocationTest, Tolerances)
{
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        const Model before = with_complements();
        Model model = before;
        if (!runs_out_with(model, set_tolerances, successes))
        {
            EXPECT_GT(successes, 0U);
            EXPECT_EQ(copy_cost(model), copy_cost(before) + 3);
            return;
        }
        EXPECT_EQ(biyos::difference(model, before), "");
        EXPECT_EQ(copy_cost(model), copy_cost(before));
    }
    ADD_FAILURE() << "the call never completed";
}

TEST(AllocationTest, Deletion)
{
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        Model before = with_complements();
        set_tolerances(before);
        Model model = before;
        if (!runs_out_with(model, delete_two, successes))
        {
            EXPECT_GT(successes, 0U);
            EXPECT_EQ(model.n_complements(), 2);
            EXPECT_EQ(copy_cost(model), copy_cost(before) - 2);
            return;
        }
        EXPECT_EQ(biyos::difference(model, before), "");
    }
    ADD_FAILURE() << "the call never completed";
}
