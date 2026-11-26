#include "core/numbers.hpp"
#include "model/functions/transpose.hpp"
#include "model/model.hpp"
#include "refusal.hpp"
#include "tolerance.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

namespace
{

using biyos::column_count;
using biyos::columns_of;
using biyos::ColumnTerms;
using biyos::Index;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Offset;
using biyos::Real;
using biyos::Variable;
using biyos::VariableKind;
using biyos::widest;
using biyos::test::expect_part;
using biyos::test::refusal;

Model with_terms()
{
    Model model;
    model.add_variables(std::vector<Variable>(4, widest(VariableKind::Continuous)));
    const std::vector<Offset> starts{0, 2, 2, 4};
    const std::vector<LinearTerm> terms{{1, 1.0}, {3, 2.0}, {1, 3.0}, {2, 4.0}};
    model.add_functions(std::vector<Real>(3, 0.0), starts, terms);
    return model;
}

void expect_values(const std::vector<Real> &found, const std::vector<Real> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        expect_part(found[k], wanted[k]);
    }
}

TEST(ColumnTest, Selected)
{
    const Model model = with_terms();
    const std::vector<Index> variables{3, 1, 0, 1};
    const ColumnTerms columns = columns_of(model, variables);
    EXPECT_EQ(columns.starts, (std::vector<Offset>{0, 1, 3, 3, 5}));
    EXPECT_EQ(columns.functions, (std::vector<Index>{0, 0, 2, 0, 2}));
    expect_values(columns.values, {2.0, 1.0, 3.0, 1.0, 3.0});
    EXPECT_EQ(column_count(model, variables), 5u);
}

TEST(ColumnTest, Range)
{
    const Model model = with_terms();
    const ColumnTerms columns = columns_of(model, 1, 3);
    EXPECT_EQ(columns.starts, (std::vector<Offset>{0, 2, 3}));
    EXPECT_EQ(columns.functions, (std::vector<Index>{0, 2, 2}));
    expect_values(columns.values, {1.0, 3.0, 4.0});
}

TEST(ColumnTest, Empty)
{
    const Model model = with_terms();
    const ColumnTerms columns = columns_of(model, {});
    EXPECT_EQ(columns.starts, (std::vector<Offset>{0}));
    EXPECT_TRUE(columns.functions.empty());
    EXPECT_TRUE(columns.values.empty());
    EXPECT_EQ(column_count(model, {}), 0u);
}

TEST(ColumnTest, Refused)
{
    const Model model = with_terms();
    const std::vector<Index> outside{0, 4};
    const std::vector<Index> negative{-1};
    EXPECT_EQ(refusal([&] { columns_of(model, outside); }),
              "invalid argument: variable 4: unknown");
    EXPECT_EQ(refusal([&] { column_count(model, negative); }),
              "invalid argument: variable -1: unknown");
    EXPECT_EQ(refusal([&] { columns_of(model, 2, 5); }), "invalid argument: variable 4: unknown");
}

}
