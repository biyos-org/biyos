#include "allocations.hpp"
#include "core/error.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using biyos::Family;
using biyos::Index;
using biyos::Model;
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

std::vector<std::string> numbered(Index count)
{
    std::vector<std::string> texts;
    for (Index k = 0; k < count; ++k)
    {
        texts.push_back("v" + std::to_string(k));
    }
    return texts;
}

Model named(Index count)
{
    Model model;
    const auto size = static_cast<std::size_t>(count) + 2;
    model.add_variables(std::vector<Variable>(size, biyos::widest(VariableKind::Continuous)));
    std::vector<Index> indices;
    for (Index k = 0; k < count; ++k)
    {
        indices.push_back(k);
    }
    const std::vector<std::string> texts = numbered(count);
    model.set_names(Family::Variable,
                    indices,
                    std::vector<std::string_view>(texts.begin(), texts.end()));
    return model;
}

void rename(Model &model, Index count)
{
    const std::vector<Index> indices{0, 1, 2, count, count, count + 1};
    const std::vector<std::string_view> names{"v1", "v0", kLong, "e", "f", "g"};
    model.set_names(Family::Variable, indices, names);
}

bool runs_out(Model &model, Index count, std::size_t successes)
{
    const PendingFailure failure(successes);
    try
    {
        rename(model, count);
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
    for (Index k = 0; k < model.variable_count(); ++k)
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

std::vector<std::optional<Index>> lookups(const Model &model, Index count)
{
    std::vector<std::string> names = numbered(count);
    names.insert(names.end(), {"e", "f", "g", std::string(kLong)});
    std::vector<std::optional<Index>> found;
    for (const std::string &name : names)
    {
        found.push_back(lookup(model, name));
    }
    return found;
}

void expect_kept(Index count)
{
    Model wanted = named(count);
    rename(wanted, count);
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        Model model = named(count);
        const Model before = model;
        const std::string *kept = model.name_of(Family::Variable, count - 1);
        if (!runs_out(model, count, successes))
        {
            EXPECT_GT(successes, 0U);
            EXPECT_EQ(names_of(model), names_of(wanted));
            EXPECT_EQ(lookups(model, count), lookups(wanted, count));
            return;
        }
        EXPECT_EQ(names_of(model), names_of(before));
        EXPECT_EQ(lookups(model, count), lookups(before, count));
        EXPECT_EQ(model.name_of(Family::Variable, count - 1), kept);
        rename(model, count);
        EXPECT_EQ(names_of(model), names_of(wanted));
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

}

TEST(AllocationTest, Renaming)
{
    for (Index count = 4; count <= 48; ++count)
    {
        SCOPED_TRACE(count);
        expect_kept(count);
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
    model.del_names(Family::Variable, indices);
    EXPECT_EQ(copy_cost(model), cost);
}
