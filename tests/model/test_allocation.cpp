#include "allocations.hpp"
#include "core/error.hpp"
#include "core/index.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using biyos::Family;
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
