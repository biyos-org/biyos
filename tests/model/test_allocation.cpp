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
    Model mdl;
    const auto size = static_cast<std::size_t>(n) + 2;
    mdl.add_vars(std::vector<Variable>(size, biyos::widest(VariableKind::Continuous)));
    std::vector<Index> idx;
    for (Index k = 0; k < n; ++k)
    {
        idx.push_back(k);
    }
    const std::vector<std::string> texts = numbered(n);
    mdl.set_names(Family::Variable, idx, std::vector<std::string_view>(texts.begin(), texts.end()));
    return mdl;
}

void rename(Model &mdl, Index n)
{
    const std::vector<Index> idx{0, 1, 2, n, n, n + 1};
    const std::vector<std::string_view> names{"v1", "v0", kLong, "e", "f", "g"};
    mdl.set_names(Family::Variable, idx, names);
}

bool runs_out(Model &mdl, Index n, std::size_t successes)
{
    const PendingFailure failure(successes);
    try
    {
        rename(mdl, n);
    }
    catch (const std::bad_alloc &)
    {
        return true;
    }
    return false;
}

std::vector<std::string> names_of(const Model &mdl)
{
    std::vector<std::string> found;
    for (Index k = 0; k < mdl.n_vars(); ++k)
    {
        const std::string *name = mdl.name_of(Family::Variable, k);
        found.push_back(name == nullptr ? "none" : *name);
    }
    return found;
}

std::optional<Index> lookup(const Model &mdl, const std::string &name)
{
    try
    {
        return mdl.idx_of(Family::Variable, name);
    }
    catch (const biyos::Error &)
    {
        return std::nullopt;
    }
}

std::vector<std::optional<Index>> lookups(const Model &mdl, Index n)
{
    std::vector<std::string> names = numbered(n);
    names.insert(names.end(), {"e", "f", "g", std::string(kLong)});
    std::vector<std::optional<Index>> found;
    for (const std::string &name : names)
    {
        found.push_back(lookup(mdl, name));
    }
    return found;
}

void expect_kept(Index n)
{
    Model wanted = named(n);
    rename(wanted, n);
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        Model mdl = named(n);
        const Model before = mdl;
        const std::string *kept = mdl.name_of(Family::Variable, n - 1);
        if (!runs_out(mdl, n, successes))
        {
            EXPECT_GT(successes, 0U);
            EXPECT_EQ(names_of(mdl), names_of(wanted));
            EXPECT_EQ(lookups(mdl, n), lookups(wanted, n));
            return;
        }
        EXPECT_EQ(names_of(mdl), names_of(before));
        EXPECT_EQ(lookups(mdl, n), lookups(before, n));
        EXPECT_EQ(mdl.name_of(Family::Variable, n - 1), kept);
        rename(mdl, n);
        EXPECT_EQ(names_of(mdl), names_of(wanted));
    }
    ADD_FAILURE() << "the call never completed";
}

Model with_terms()
{
    Model mdl;
    mdl.add_vars(std::vector<Variable>(4, biyos::widest(VariableKind::Continuous)));
    const std::vector<double> constants(3, 0.0);
    const std::vector<Offset> starts{0, 2, 2, 3};
    mdl.add_funs(constants, starts, {{1, 1.0}, {3, 2.0}, {2, 3.0}});
    return mdl;
}

bool runs_out_adding(Model &mdl, std::size_t successes)
{
    const std::vector<Index> funs{2, 0};
    const std::vector<Offset> starts{0, 2, 4};
    std::vector<LinearTerm> terms{{0, 1.0}, {2, 1.0}, {0, 1.0}, {2, 1.0}};
    const PendingFailure failure(successes);
    try
    {
        mdl.add_lin_terms(funs, starts, std::move(terms));
    }
    catch (const std::bad_alloc &)
    {
        return true;
    }
    return false;
}

std::size_t lookup_cost(const Model &mdl)
{
    const std::size_t start = biyos::test::allocations();
    EXPECT_EQ(mdl.idx_of(Family::Variable, kLong), 2);
    EXPECT_NE(mdl.name_of(Family::Variable, 2), nullptr);
    return biyos::test::allocations() - start;
}

std::size_t copy_cost(const Model &mdl)
{
    const std::size_t start = biyos::test::allocations();
    const Model copy = mdl;
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
    Model mdl = named(4);
    rename(mdl, 4);
    EXPECT_EQ(lookup_cost(mdl), 0U);
}

TEST(AllocationTest, NameRelease)
{
    Model mdl = named(1);
    const std::size_t cost = copy_cost(mdl);
    const std::vector<Index> idx{1};
    const std::vector<std::string_view> names{kLong};
    mdl.set_names(Family::Variable, idx, names);
    mdl.del_names(Family::Variable, idx);
    EXPECT_EQ(copy_cost(mdl), cost);
}

TEST(AllocationTest, LinearTerms)
{
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        Model mdl = with_terms();
        if (!runs_out_adding(mdl, successes))
        {
            EXPECT_GT(successes, 0U);
            EXPECT_EQ(mdl.terms(0, 3).size(), 6U);
            return;
        }
        EXPECT_EQ(biyos::difference(mdl, with_terms()), "");
    }
    ADD_FAILURE() << "the call never completed";
}
