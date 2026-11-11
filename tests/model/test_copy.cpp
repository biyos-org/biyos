#include "core/index.hpp"
#include "expr/terms.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"
#include "sample.hpp"

#include <gtest/gtest.h>

#include <span>
#include <vector>

namespace
{

using biyos::Constraint;
using biyos::difference;
using biyos::Index;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Offset;
using biyos::QuadraticTerm;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::built;
using biyos::test::sample;

void change(Model &model)
{
    const Index index = 1;
    const Variable variable{.kind = VariableKind::Integer, .lower = 0.0, .upper = 1.0};
    const Constraint constraint{.function = 2, .lower = -5.0, .upper = 5.0};
    const std::vector<double> constants{4.0};
    const std::vector<Offset> starts{0, 1};
    model.set_variables(std::span(&index, 1), std::span(&variable, 1));
    model.add_variables(std::span(&variable, 1));
    model.add_functions(constants, starts, {LinearTerm{.variable = 3, .coefficient = 1.0}});
    const QuadraticTerm pair{.first = 3, .second = 1, .coefficient = 2.0};
    model.add_quadratic_terms(std::span(&index, 1), starts, {pair});
    model.set_constraints(std::span(&index, 1), std::span(&constraint, 1));
    model.add_constraints(std::span(&constraint, 1));
}

}

TEST(CopyTest, Equal)
{
    const Model source = built(sample());
    const Model copy = source;
    EXPECT_EQ(difference(source, copy), "");
    EXPECT_EQ(difference(copy, built(sample())), "");
}

TEST(CopyTest, Assigned)
{
    const Model source = built(sample());
    Model copy;
    copy = source;
    EXPECT_EQ(difference(source, copy), "");
}

TEST(CopyTest, Independent)
{
    const Model source = built(sample());
    Model copy = source;
    change(copy);
    EXPECT_EQ(difference(source, built(sample())), "");
    EXPECT_EQ(difference(source, copy), "variable count 3 against 4");
    Model changed = built(sample());
    change(changed);
    EXPECT_EQ(difference(copy, changed), "");
}
