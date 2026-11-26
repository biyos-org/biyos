#include "reading.hpp"
#include "samples.hpp"
#include "writing.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

using biyos::Constraint;
using biyos::difference;
using biyos::Family;
using biyos::Index;
using biyos::Indicator;
using biyos::LinearTerm;
using biyos::Member;
using biyos::Model;
using biyos::ObjectiveSense;
using biyos::Offset;
using biyos::Real;
using biyos::SpecialOrderedSetKind;
using biyos::Variable;
using biyos::VariableKind;
using biyos::test::add_columns;
using biyos::test::add_goal;
using biyos::test::add_named;
using biyos::test::add_rows;
using biyos::test::bad;
using biyos::test::continuous;
using biyos::test::fault_of;
using biyos::test::kIndicatorsMpsFile;
using biyos::test::kInfinity;
using biyos::test::kSetsMpsFile;
using biyos::test::printed;
using biyos::test::read_sample;
using biyos::test::text;

namespace
{

std::string with_sets(std::initializer_list<std::string_view> sets)
{
    const std::string head =
        text({"NAME t", "ROWS", " N f", "COLUMNS", " x f 1", " y f 1", "RHS", "SOS"});
    return head + text(sets) + "ENDATA\n";
}

std::string with_indicators(std::initializer_list<std::string_view> indicators)
{
    const std::string head = text({
        "NAME t",
        "ROWS",
        " N f",
        " L c",
        " G d",
        "COLUMNS",
        " x f 1 c 1",
        " x d 1",
        " b c 1",
        "RHS",
        "RANGES",
        " g d 1",
        "BOUNDS",
        " BV v b",
        "INDICATORS",
    });
    return head + text(indicators) + "ENDATA\n";
}

Variable binary()
{
    return Variable{.kind = VariableKind::Binary, .lower = 0.0, .upper = 1.0};
}

Indicator indicator(Index variable, int value, Index function, Real lower, Real upper)
{
    return Indicator{
        .variable = variable,
        .value = value,
        .function = function,
        .lower = lower,
        .upper = upper,
    };
}

}

TEST(MpsTest, Sets)
{
    Model wanted;
    wanted.set_name("sets");
    add_columns(wanted,
                {continuous(0.0, 4.0), continuous(0.0, kInfinity), continuous(0.0, kInfinity)},
                {"x", "y", "z"});
    add_rows(wanted,
             {0.0},
             {0, 3},
             {
                 LinearTerm{.variable = 0, .coefficient = 1.0},
                 LinearTerm{.variable = 1, .coefficient = 1.0},
                 LinearTerm{.variable = 2, .coefficient = 1.0},
             });
    add_goal(wanted, 0, ObjectiveSense::Minimize, "obj");
    const std::vector kinds{
        SpecialOrderedSetKind::One,
        SpecialOrderedSetKind::Two,
        SpecialOrderedSetKind::One,
    };
    const std::vector<Offset> starts{0, 2, 5, 5};
    const std::vector<Member> members{
        Member{.variable = 0, .weight = 1.5},
        Member{.variable = 1, .weight = 3.0},
        Member{.variable = 1, .weight = 1.0},
        Member{.variable = 2, .weight = 2.0},
        Member{.variable = 0, .weight = 3.0},
    };
    wanted.add_special_ordered_sets(kinds, starts, members);
    const std::vector<Index> named{0, 2};
    const std::vector<std::string_view> names{"pick", "empty"};
    wanted.set_names(Family::SpecialOrderedSet, named, names);
    EXPECT_EQ(difference(read_sample(kSetsMpsFile), wanted), "");
}

TEST(MpsTest, SetFaults)
{
    EXPECT_EQ(fault_of(with_sets({" S1 s", "    w 1"})), bad(10, "unknown column 'w'"));
    EXPECT_EQ(fault_of(with_sets({"    x 1"})), bad(9, "a set member comes before its set"));
    EXPECT_EQ(fault_of(with_sets({" S1 s", "    x 1", "    x 2"})),
              bad(11, "column 'x' is repeated in the set"));
    EXPECT_EQ(fault_of(with_sets({" S2 s", "    x 2", "    y"})),
              bad(11, "the weight of column 'y' is repeated in the set"));
    EXPECT_EQ(fault_of(with_sets({" S1 s t"})), bad(9, "a set line holds a type and a name"));
    EXPECT_EQ(fault_of(with_sets({" S1 s", "    x 1 2"})),
              bad(10, "a set member holds a column and a weight"));
    EXPECT_EQ(fault_of(text({"NAME t", "ROWS", "COLUMNS", "SOS", "BOUNDS", "ENDATA"})),
              bad(5, "section BOUNDS is out of order"));
}

TEST(MpsTest, PrintedSets)
{
    const std::string text = printed(read_sample(kSetsMpsFile));
    EXPECT_TRUE(text.ends_with("BOUNDS\n"
                               " UP BND  x  4\n"
                               "SOS\n"
                               " S1 pick\n"
                               "    x  1.5\n"
                               "    y  3\n"
                               " S2\n"
                               "    y  1\n"
                               "    z  2\n"
                               "    x  3\n"
                               " S1 empty\n"
                               "ENDATA\n"))
        << text;
}

TEST(MpsTest, Indicators)
{
    Model wanted;
    wanted.set_name("indicators");
    add_columns(wanted,
                {binary(), binary(), continuous(0.0, kInfinity), continuous(0.0, kInfinity)},
                {"z", "b", "x", "y"});
    add_rows(wanted,
             {0.0, 0.0, 0.0, 0.0},
             {0, 3, 4, 6, 8},
             {
                 LinearTerm{.variable = 0, .coefficient = 1.0},
                 LinearTerm{.variable = 1, .coefficient = 2.0},
                 LinearTerm{.variable = 2, .coefficient = 1.0},
                 LinearTerm{.variable = 2, .coefficient = 1.0},
                 LinearTerm{.variable = 2, .coefficient = 1.0},
                 LinearTerm{.variable = 3, .coefficient = 1.0},
                 LinearTerm{.variable = 2, .coefficient = 1.0},
                 LinearTerm{.variable = 3, .coefficient = 2.0},
             });
    add_named(wanted, {Constraint{.function = 1, .lower = 2.0, .upper = 2.0}}, {"fix"});
    add_goal(wanted, 0, ObjectiveSense::Minimize, "obj");
    const std::vector indicators{
        indicator(0, 1, 2, -kInfinity, 4.0),
        indicator(1, 0, 3, 1.0, kInfinity),
    };
    wanted.add_indicators(indicators);
    EXPECT_EQ(difference(read_sample(kIndicatorsMpsFile), wanted), "");
}

TEST(MpsTest, SharedRow)
{
    const Model model = read_sample(with_indicators({" IF c b 1", " IF c b 0"}));
    ASSERT_EQ(model.n_indicators(), 2);
    EXPECT_EQ(model.indicator(0).function, 1);
    EXPECT_EQ(model.indicator(1).function, 1);
    EXPECT_EQ(model.indicator(1).value, 0);
    EXPECT_EQ(model.n_constraints(), 1);
    EXPECT_EQ(model.constraint(0).function, 2);
}

TEST(MpsTest, IndicatorFaults)
{
    EXPECT_EQ(fault_of(with_indicators({" IF c w 1"})), bad(16, "unknown column 'w'"));
    EXPECT_EQ(fault_of(with_indicators({" IF c x 1"})), bad(16, "column 'x' is not binary"));
    EXPECT_EQ(fault_of(with_indicators({" IF e b 1"})), bad(16, "unknown row 'e'"));
    EXPECT_EQ(fault_of(with_indicators({" IF f b 1"})),
              bad(16, "N row 'f' cannot hold an indicator"));
    EXPECT_EQ(fault_of(with_indicators({" IF d b 1"})),
              bad(16, "row 'd' has a range, so it cannot hold an indicator"));
    EXPECT_EQ(fault_of(with_indicators({" IF c b 2"})),
              bad(16, "indicator value '2' is not 0 or 1"));
    EXPECT_EQ(fault_of(with_indicators({" IF c b"})),
              bad(16, "an indicator line holds IF, a row, a column and a value"));
    EXPECT_EQ(fault_of(with_indicators({" THEN c b 1"})),
              bad(16, "an indicator line holds IF, a row, a column and a value"));
}

TEST(MpsTest, IntegerSwitch)
{
    const std::string file = text({
        "NAME t",
        "ROWS",
        " N f",
        " L c",
        "COLUMNS",
        " MARKER 'MARKER' 'INTORG'",
        " z c 1",
        " MARKER 'MARKER' 'INTEND'",
        "RHS",
        "BOUNDS",
        " UP v z 2",
        "INDICATORS",
        " IF c z 1",
        "ENDATA",
    });
    EXPECT_EQ(fault_of(file), bad(13, "column 'z' is not binary"));
}
