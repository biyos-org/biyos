#include "reading.hpp"
#include "samples.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

using biyos::difference;
using biyos::Family;
using biyos::Index;
using biyos::LinearTerm;
using biyos::Member;
using biyos::Model;
using biyos::ObjectiveSense;
using biyos::Offset;
using biyos::SpecialOrderedSetKind;
using biyos::test::add_columns;
using biyos::test::add_goal;
using biyos::test::add_rows;
using biyos::test::bad;
using biyos::test::continuous;
using biyos::test::fault_of;
using biyos::test::kInfinity;
using biyos::test::kSetsMpsFile;
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
