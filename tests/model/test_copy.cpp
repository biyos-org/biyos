#include "core/index.hpp"
#include "expr/terms.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"
#include "sample.hpp"

#include <gtest/gtest.h>

#include <span>
#include <string_view>
#include <vector>

namespace
{

using biyos::Constraint;
using biyos::difference;
using biyos::Family;
using biyos::Index;
using biyos::LinearTerm;
using biyos::Model;
using biyos::Objective;
using biyos::Offset;
using biyos::QuadraticTerm;
using biyos::Sense;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::built;
using biyos::test::sample;

void change(Model &mdl)
{
    const Index idx = 1;
    const Variable var{.kind = VariableKind::Integer, .lower = 0.0, .upper = 1.0};
    const Constraint con{.function = 2, .lower = -5.0, .upper = 5.0};
    const std::vector<double> constants{4.0};
    const std::vector<Offset> starts{0, 1};
    mdl.set_vars(std::span(&idx, 1), std::span(&var, 1));
    mdl.add_vars(std::span(&var, 1));
    mdl.add_funs(constants, starts, {LinearTerm{.variable = 3, .coefficient = 1.0}});
    const QuadraticTerm pair{.row = 3, .column = 1, .coefficient = 2.0};
    mdl.add_quad_terms(std::span(&idx, 1), starts, {pair});
    mdl.set_cons(std::span(&idx, 1), std::span(&con, 1));
    mdl.add_cons(std::span(&con, 1));
    const Objective obj{
        .function = 3,
        .sense = Sense::Maximize,
        .priority = 2,
        .weight = 4.0,
        .absolute = 1.0,
        .relative = 0.0,
    };
    mdl.set_objs(std::span(&idx, 1), std::span(&obj, 1));
    mdl.add_objs(std::span(&obj, 1));
    const std::vector<Index> renamed{0, 1};
    const std::vector<std::string_view> names{"y", "w"};
    for (const Family family : biyos::kFamilies)
    {
        mdl.set_names(family, renamed, names);
    }
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
