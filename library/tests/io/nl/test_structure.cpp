#include "building.hpp"
#include "loading.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <span>
#include <string>
#include <string_view>
#include <vector>

using biyos::Complement;
using biyos::Constraint;
using biyos::difference;
using biyos::Index;
using biyos::LinearTerm;
using biyos::Model;
using biyos::ObjectiveSense;
using biyos::Offset;
using biyos::test::nl::add_goal;
using biyos::test::nl::bad;
using biyos::test::nl::continuous;
using biyos::test::nl::fault_of;
using biyos::test::nl::kInfinity;
using biyos::test::nl::read_sample;
using biyos::test::nl::refused;
using biyos::test::nl::square_of;
using biyos::test::nl::tiny_fault;

namespace
{

constexpr std::string_view kNonnegativeNlFile = R"(g3 1 1 0
 2 2 1 0 0
 0 0 1 0 0 0
 0 0
 0 0 0
 0 0 0 1
 0 0 0 0 0
 4 1
 0 0
 0 0 0 0 0
C0
n-1
C1
n0
O0 0
n0
r
5 1 1
1 4
b
2 0
3
k1
2
J0 2
0 1
1 1
J1 2
0 1
1 1
G0 1
1 1
)";

constexpr std::string_view kMixedNlFile = R"(g3 1 1 0
 2 3 1 1 0
 1 0 1 1 1 1
 0 0
 1 0 0
 0 0 0 1
 0 0 0 0 0
 5 1
 0 0
 0 0 0 0 0
C0
o5
v0
n2
C1
n0
C2
n0
O0 0
n0
r
5 3 2
5 2 1
0 -4 4
b
1 3
0 -1 2
k1
2
J0 1
1 -1
J1 2
0 1
1 1
J2 2
0 1
1 1
G0 1
0 1
)";

constexpr std::string_view kTwiceNlFile = R"(g3 1 1 0
 1 2 0 0 0
 0 0 2 0 0 0
 0 0
 0 0 0
 0 0 0 1
 0 0 0 0 0
 0 0
 0 0
 0 0 0 0 0
C0
n0
C1
n1
r
5 1 1
5 1 1
b
2 0
)";

constexpr std::string_view kPairedHeader = R"(g3 1 1 0
 1 1 1 0 0
 0 0 1 0 0 0
 0 0
 0 0 0
 0 0 0 1
 0 0 0 0 0
 1 1
 0 0
 0 0 0 0 0
)";

constexpr std::string_view kCrowdedHeader = R"(g3 1 1 0
 1 1 1 0 0
 0 0 1 1 0 0
 0 0
 0 0 0
 0 0 0 1
 0 0 0 0 0
 1 1
 0 0
 0 0 0 0 0
)";

constexpr std::string_view kUnknownCodeTail = R"(r
5 4 1
)";

constexpr std::string_view kOutsideTail = R"(r
5 1 2
)";

constexpr std::string_view kBoundTail = R"(b
5 1 1
)";

constexpr std::string_view kMismatchTail = R"(C0
n0
O0 0
n0
r
5 1 1
b
3
)";

Model nonnegative()
{
    Model model;
    model.add_variables(std::vector{continuous(0.0, kInfinity), continuous(-kInfinity, kInfinity)});
    model.add_functions(std::vector{-1.0, 0.0, 0.0, 0.0},
                        std::vector<Offset>{0, 2, 4, 5, 6},
                        {
                            LinearTerm{.variable = 0, .coefficient = 1.0},
                            LinearTerm{.variable = 1, .coefficient = 1.0},
                            LinearTerm{.variable = 0, .coefficient = 1.0},
                            LinearTerm{.variable = 1, .coefficient = 1.0},
                            LinearTerm{.variable = 1, .coefficient = 1.0},
                            LinearTerm{.variable = 0, .coefficient = 1.0},
                        });
    model.add_constraints(
        std::vector{Constraint{.function = 1, .lower = -kInfinity, .upper = 4.0}});
    add_goal(model, 2, ObjectiveSense::Minimize);
    model.add_complements(
        std::vector{Complement{.function = 3, .lower = 0.0, .upper = kInfinity, .multiplier = 0}});
    return model;
}

Model mixed()
{
    Model model;
    model.add_variables(std::vector{continuous(-kInfinity, 3.0), continuous(-1.0, 2.0)});
    model.add_functions(std::vector{0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                        std::vector<Offset>{0, 1, 3, 5, 6, 7, 8},
                        {
                            LinearTerm{.variable = 1, .coefficient = -1.0},
                            LinearTerm{.variable = 0, .coefficient = 1.0},
                            LinearTerm{.variable = 1, .coefficient = 1.0},
                            LinearTerm{.variable = 0, .coefficient = 1.0},
                            LinearTerm{.variable = 1, .coefficient = 1.0},
                            LinearTerm{.variable = 0, .coefficient = 1.0},
                            LinearTerm{.variable = 1, .coefficient = 1.0},
                            LinearTerm{.variable = 0, .coefficient = 1.0},
                        });
    const Offset root = square_of(model, model.add_variable_node(0));
    const Index function = 0;
    model.attach_expressions(std::span(&function, 1), std::span(&root, 1));
    model.add_constraints(std::vector{Constraint{.function = 2, .lower = -4.0, .upper = 4.0}});
    add_goal(model, 3, ObjectiveSense::Minimize);
    model.add_complements(std::vector{
        Complement{.function = 4, .lower = -1.0, .upper = 2.0, .multiplier = 0},
        Complement{.function = 5, .lower = -kInfinity, .upper = 3.0, .multiplier = 1},
    });
    return model;
}

}

TEST(NlTest, Nonnegative)
{
    EXPECT_EQ(difference(read_sample(kNonnegativeNlFile), nonnegative()), "");
}

TEST(NlTest, Mixed)
{
    EXPECT_EQ(difference(read_sample(kMixedNlFile), mixed()), "");
}

TEST(NlTest, ComplementCode)
{
    EXPECT_EQ(tiny_fault(kUnknownCodeTail), bad(12, "complementarity code '4' is unknown"));
}

TEST(NlTest, PairRange)
{
    EXPECT_EQ(tiny_fault(kOutsideTail), bad(12, "complemented variable '2' is out of range"));
}

TEST(NlTest, BoundPairing)
{
    EXPECT_EQ(tiny_fault(kBoundTail), bad(12, "a variable bound cannot be a complementarity"));
}

TEST(NlTest, Mismatch)
{
    EXPECT_EQ(fault_of(std::string(kPairedHeader) + std::string(kMismatchTail)),
              bad(18, "complementarity code 1 does not match the bounds of variable 0"));
}

TEST(NlTest, PairCount)
{
    EXPECT_EQ(tiny_fault(kMismatchTail),
              bad(16, "the header counts 0 complements, the segment holds 1"));
}

TEST(NlTest, CrowdedHeader)
{
    EXPECT_EQ(fault_of(kCrowdedHeader),
              bad(10, "the header counts more complements than constraints"));
}

TEST(NlTest, PairedTwice)
{
    EXPECT_EQ(fault_of(kTwiceNlFile), refused(19, "variable 0 is complemented twice"));
}
