#include "core/numbers.hpp"
#include "model/model.hpp"
#include "model/utils/gather.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <span>
#include <vector>

namespace
{

using biyos::Gathered;
using biyos::gathered;
using biyos::Index;
using biyos::item_count;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Offset;
using biyos::Real;
using biyos::to_size;
using biyos::Variable;
using biyos::VariableKind;
using biyos::widest;
using biyos::test::refusal;

Model with_terms()
{
    Model model;
    model.add_variables(std::vector<Variable>(3, widest(VariableKind::Continuous)));
    const std::vector<Offset> starts{0, 2, 2, 3};
    const std::vector<LinearTerm> terms{{0, 1.0}, {2, 2.0}, {1, 3.0}};
    model.add_functions(std::vector<Real>(3, 0.0), starts, terms);
    return model;
}

std::vector<Index> variables_of(std::span<const LinearTerm> terms)
{
    std::vector<Index> variables;
    for (const LinearTerm &term : terms)
    {
        variables.push_back(term.variable);
    }
    return variables;
}

TEST(GatherTest, Parts)
{
    const Model model = with_terms();
    const std::vector<Index> functions{2, 1, 0, 2};
    const Gathered<LinearTerm> rows = gathered(model, &Model::linear_terms, functions);
    EXPECT_EQ(rows.starts, (std::vector<Offset>{0, 1, 1, 3, 4}));
    EXPECT_EQ(variables_of(rows.items), (std::vector<Index>{1, 0, 2, 1}));
    EXPECT_EQ(item_count(model, &Model::linear_terms, functions), 4u);
}

TEST(GatherTest, Empty)
{
    const Model model = with_terms();
    const Gathered<LinearTerm> rows = gathered(model, &Model::linear_terms, {});
    EXPECT_EQ(rows.starts, (std::vector<Offset>{0}));
    EXPECT_TRUE(rows.items.empty());
    EXPECT_EQ(item_count(model, &Model::linear_terms, {}), 0u);
}

TEST(GatherTest, Read)
{
    const std::vector<Index> items{4, 5, 6};
    const auto read = [&](Index k) { return std::span(items).first(to_size(k)); };
    const std::vector<Index> records{2, 0, 3};
    const Gathered<Index> found = gathered(records, read);
    EXPECT_EQ(found.starts, (std::vector<Offset>{0, 2, 2, 5}));
    EXPECT_EQ(found.items, (std::vector<Index>{4, 5, 4, 5, 6}));
}

TEST(GatherTest, Refused)
{
    const Model model = with_terms();
    const std::vector<Index> outside{0, 3};
    EXPECT_EQ(refusal([&] { gathered(model, &Model::linear_terms, outside); }),
              "invalid argument: function 3: unknown");
    EXPECT_EQ(refusal([&] { item_count(model, &Model::linear_terms, outside); }),
              "invalid argument: function 3: unknown");
}

}
