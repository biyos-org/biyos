#include "biyos/biyos.h"
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

using biyos::Complement;
using biyos::ConeKind;
using biyos::Constraint;
using biyos::difference;
using biyos::Family;
using biyos::Index;
using biyos::Indicator;
using biyos::LinearTerm;
using biyos::Member;
using biyos::Model;
using biyos::Objective;
using biyos::Offset;
using biyos::QuadraticTerm;
using biyos::Sense;
using biyos::SosKind;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::built;
using biyos::test::sample;

void change(Model &model)
{
    const Index index = 1;
    const Variable variable{.kind = VariableKind::Binary, .lower = 1.0, .upper = 1.0};
    const Constraint constraint{.function = 2, .lower = -5.0, .upper = 5.0};
    const std::vector<double> constants{4.0};
    const std::vector<Offset> starts{0, 1};
    model.set_variables(std::span(&index, 1), std::span(&variable, 1));
    model.add_variables(std::span(&variable, 1));
    model.add_functions(constants, starts, {LinearTerm{.variable = 3, .coefficient = 1.0}});
    const QuadraticTerm pair{.row = 3, .column = 1, .coefficient = 2.0};
    model.add_quadratic_terms(std::span(&index, 1), starts, {pair});
    const Index added = 3;
    const Offset exponential = 2;
    const Offset sine = model.add_node(BIYOS_OP_SIN, std::span(&exponential, 1), {});
    model.attach_expressions(std::span(&added, 1), std::span(&sine, 1));
    model.set_constraints(std::span(&index, 1), std::span(&constraint, 1));
    model.add_constraints(std::span(&constraint, 1));
    const Objective objective{
        .function = 3,
        .sense = Sense::Maximize,
        .priority = 2,
        .weight = 4.0,
        .absolute = 1.0,
        .relative = 0.0,
    };
    model.set_objectives(std::span(&index, 1), std::span(&objective, 1));
    model.add_objectives(std::span(&objective, 1));
    const Indicator indicator{
        .variable = 3,
        .value = 0,
        .function = 3,
        .lower = -1.0,
        .upper = 2.0,
    };
    model.set_indicators(std::span(&index, 1), std::span(&indicator, 1));
    model.add_indicators(std::span(&indicator, 1));
    const SosKind kind = SosKind::Sos1;
    const std::vector<Offset> members{0, 2};
    const std::vector<Member> sos{{.variable = 3, .weight = 1.0}, {.variable = 1, .weight = 2.0}};
    model.add_sos(std::span(&kind, 1), members, sos);
    const ConeKind cone = ConeKind::Exponential;
    const std::vector<Offset> ends{0, 3};
    const std::vector<Index> functions{3, 1, 0};
    model.add_cones(std::span(&cone, 1), ends, functions, std::vector<Offset>{0, 0}, {});
    const Complement complement{.function = 3, .lower = -1.0, .upper = 3.0, .multiplier = 1};
    model.set_complements(std::span(&index, 1), std::span(&complement, 1));
    model.add_complements(std::span(&complement, 1));
    const double tolerance = 0.5;
    model.set_tolerances(Family::Complement, std::span(&index, 1), std::span(&tolerance, 1), {});
    model.set_tolerances(Family::Variable, std::span(&index, 1), {}, std::span(&tolerance, 1));
    model.set_tolerances(Family::Constraint, std::span(&index, 1), std::span(&tolerance, 1), {});
    model.set_tolerances(Family::Cone, std::span(&index, 1), {}, std::span(&tolerance, 1));
    const std::vector<Index> renamed{0, 1};
    const std::vector<std::string_view> names{"y", "w"};
    for (const Family family : biyos::kFamilies)
    {
        model.set_names(family, renamed, names);
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
