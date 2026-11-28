#include "building.hpp"
#include "loading.hpp"

#include "core/numbers.hpp"
#include "expr/graph/node.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using biyos::Constraint;
using biyos::difference;
using biyos::Index;
using biyos::Model;
using biyos::ObjectiveSense;
using biyos::Offset;
using biyos::Operator;
using biyos::test::nl::add_goal;
using biyos::test::nl::bad;
using biyos::test::nl::compare_of;
using biyos::test::nl::continuous;
using biyos::test::nl::fault_of;
using biyos::test::nl::kInfinity;
using biyos::test::nl::node;
using biyos::test::nl::read_sample;
using biyos::test::nl::tiny_fault;
using biyos::test::nl::tiny_with;

namespace
{

constexpr std::string_view kFirstLine = "g3 1 1 0\n 1 1 1 0 0 ";

constexpr std::string_view kOtherLines = R"(
 0 0
 0 0
 0 0 0
 0 0 0 1
 0 0 0 0 0
 1 1
 0 0
 0 0 0 0 0
)";

constexpr std::string_view kFrame = R"(C0
n0
O0 0
n0
r
3
b
3
)";

constexpr std::string_view kDisjunction = R"(L0
o20
o23
v0
n-1
o28
v0
n1
)";

constexpr std::string_view kNested = R"(L0
o21
o20
o23
v0
n-1
o28
v0
n1
o34
o24
v0
n2
)";

constexpr std::string_view kNumber = R"(L0
v0
)";

constexpr std::string_view kSquare = R"(C0
n0
O0 0
o5
v0
n2
r
3
b
3
)";

std::string logical_file(std::string_view count, std::string_view tail)
{
    return std::string(kFirstLine) + std::string(count) + std::string(kOtherLines) +
           std::string(tail);
}

std::string one_logical(std::string_view segment)
{
    return logical_file("1", std::string(kFrame) + std::string(segment));
}

Model logical_wanted(Offset (*build)(Model &, Offset))
{
    Model model;
    model.add_variables(std::vector{continuous(-kInfinity, kInfinity)});
    model.add_functions(std::vector{0.0, 0.0, 0.0}, std::vector<Offset>{0, 0, 0, 0}, {});
    const Offset root = build(model, model.add_variable_node(0));
    const Index function = 2;
    model.attach_expressions(std::span(&function, 1), std::span(&root, 1));
    const std::array constraints{
        Constraint{.function = 0, .lower = -kInfinity, .upper = kInfinity},
        Constraint{.function = 2, .lower = 1.0, .upper = 1.0},
    };
    model.add_constraints(constraints);
    add_goal(model, 1, ObjectiveSense::Minimize);
    return model;
}

Offset either_of(Model &model, Offset x)
{
    const Offset below = compare_of(model, Operator::LessEqual, x, -1.0);
    const Offset above = compare_of(model, Operator::GreaterEqual, x, 1.0);
    return node(model, Operator::Or, {below, above}, {});
}

Offset nested_of(Model &model, Offset x)
{
    const Offset either = either_of(model, x);
    const Offset two = compare_of(model, Operator::Equal, x, 2.0);
    const Offset other = node(model, Operator::Not, {two}, {});
    return node(model, Operator::And, {either, other}, {});
}

Offset nonzero_of(Model &model, Offset x)
{
    return compare_of(model, Operator::NotEqual, x, 0.0);
}

}

TEST(NlTest, Disjunction)
{
    const Model model = read_sample(one_logical(kDisjunction));
    EXPECT_EQ(difference(model, logical_wanted(either_of)), "");
}

TEST(NlTest, NestedLogic)
{
    const Model model = read_sample(one_logical(kNested));
    EXPECT_EQ(difference(model, logical_wanted(nested_of)), "");
}

TEST(NlTest, NumberLogic)
{
    const Model model = read_sample(one_logical(kNumber));
    EXPECT_EQ(difference(model, logical_wanted(nonzero_of)), "");
}

TEST(NlTest, NoLogic)
{
    const Model model = read_sample(logical_file("0", kSquare));
    EXPECT_EQ(difference(model, read_sample(tiny_with(kSquare))), "");
}

TEST(NlTest, LogicRange)
{
    EXPECT_EQ(tiny_fault(kNumber), bad(11, "logical constraint 0 is out of range"));
}

TEST(NlTest, LogicTwice)
{
    const std::string file = one_logical(std::string(kNumber) + std::string(kNumber));
    EXPECT_EQ(fault_of(file), bad(21, "logical constraint 0 is repeated"));
}

TEST(NlTest, LogicMissing)
{
    EXPECT_EQ(fault_of(logical_file("1", kFrame)),
              bad(18, "logical constraint 0 has no expression"));
}

TEST(NlTest, LogicRoom)
{
    const std::string file = logical_file("20000000", "");
    EXPECT_EQ(fault_of(file), bad(10, "the header counts more records than the file holds"));
}

TEST(NlTest, LogicFunctions)
{
    const std::string file = logical_file("2147483647", "");
    EXPECT_EQ(fault_of(file), bad(10, "the header counts more functions than a model holds"));
}
