#include "reading.hpp"
#include "samples.hpp"

#include "io/mps/mps.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using biyos::difference;
using biyos::Family;
using biyos::Model;
using biyos::MpsForm;
using biyos::test::bad;
using biyos::test::fault_of;
using biyos::test::kLpMpsFile;
using biyos::test::read_sample;
using biyos::test::rename;
using biyos::test::text;
using biyos::test::warnings_of;

namespace
{

constexpr std::string_view kLpFixedFile = R"(* A small linear program
NAME          small
ROWS
 N  cost
 L  limit
 G  floor
 E  balance
COLUMNS
    x         cost      1.0            limit     2.0
    x         balance   1.0
    y         cost      -3.5           floor     1.0
    y         limit     1.0            balance   -1.0
RHS
    rhs       cost      4.0            limit     10.0
    rhs       floor     2.0
ENDATA
)";

constexpr std::string_view kBlankFixedFile = R"(NAME          blank
OBJSENSE
    MAX
ROWS
 N  obj
 L  cap
 G  low
 E  fix
COLUMNS
    a         obj       1.0            cap       1.0
    MARKER    'MARKER'                 'INTORG'
    b         obj       2.0            cap       1.0
    MARKER    'MARKER'                 'INTEND'
    x         fix       1.0            low       1.0
    y         low       2.0
RHS
              cap       4.0            fix       2.0
              low       1.0
RANGES
              cap       1.5
BOUNDS
 UP           x         8.0
 MI           y
 BV           a
SOS
 S1 pick
    x         1.5
    y         3.0
INDICATORS
 IF low       a         1
ENDATA
)";

constexpr std::string_view kBlankFreeFile = R"(NAME blank
OBJSENSE
    MAX
ROWS
 N obj
 L cap
 G low
 E fix
COLUMNS
    a obj 1.0 cap 1.0
    MARKER 'MARKER' 'INTORG'
    b obj 2.0 cap 1.0
    MARKER 'MARKER' 'INTEND'
    x fix 1.0 low 1.0
    y low 2.0
RHS
    rhs cap 4.0 fix 2.0
    rhs low 1.0
RANGES
    rng cap 1.5
BOUNDS
 UP bnd x 8.0
 MI bnd y
 BV bnd a
SOS
 S1 pick
    x 1.5
    y 3.0
INDICATORS
 IF low a 1
ENDATA
)";

constexpr std::string_view kSpacesFixedFile = R"(NAME          fixed model
ROWS
 N  tot cost
 L  cap one
COLUMNS
    x 1       tot cost  1.0            cap one   2.0
    y 2       cap one   1.0
RHS
    rhs       cap one   4.0
BOUNDS
 UP bnd       y 2       3.0
ENDATA
)";

constexpr std::string_view kSpacesFreeFile = R"(NAME fixed model
ROWS
 N tot_cost
 L cap_one
COLUMNS
    x_1 tot_cost 1.0 cap_one 2.0
    y_2 cap_one 1.0
RHS
    rhs cap_one 4.0
BOUNDS
 UP bnd y_2 3.0
ENDATA
)";

Model read_fixed(std::string_view text)
{
    return read_sample(text, MpsForm::Fixed);
}

}

TEST(FixedMpsTest, Linear)
{
    EXPECT_EQ(difference(read_fixed(kLpFixedFile), read_sample(kLpMpsFile)), "");
}

TEST(FixedMpsTest, BlankVector)
{
    EXPECT_EQ(difference(read_fixed(kBlankFixedFile), read_sample(kBlankFreeFile)), "");
}

TEST(FixedMpsTest, Spaces)
{
    Model wanted = read_sample(kSpacesFreeFile);
    rename(wanted, Family::Objective, {"tot cost"});
    rename(wanted, Family::Constraint, {"cap one"});
    rename(wanted, Family::Variable, {"x 1", "y 2"});
    EXPECT_EQ(difference(read_fixed(kSpacesFixedFile), wanted), "");
}

TEST(FixedMpsTest, Spill)
{
    const std::string spilled = text({
        "NAME",
        "ROWS",
        " N  cost",
        "COLUMNS",
        "    x         cost      1234567890123456",
        "ENDATA",
    });
    const std::string fault = "a column line needs a name and up to two pairs of row and value";
    EXPECT_EQ(fault_of(spilled, MpsForm::Fixed), bad(5, fault));
    EXPECT_EQ(fault_of(spilled), "");
}

TEST(FixedMpsTest, BlankSense)
{
    const std::string file = text({"NAME", "ROWS", "    cost", "ENDATA"});
    EXPECT_EQ(fault_of(file, MpsForm::Fixed), bad(3, "a row needs a sense and a name"));
}

TEST(FixedMpsTest, SenseWord)
{
    const std::string file = text({
        "NAME",
        "OBJSENSE",
        "  MAX",
        "ROWS",
        " N  cost",
        "COLUMNS",
        "    x         cost      1.0",
        "ENDATA",
    });
    EXPECT_EQ(difference(read_fixed(file), read_sample(file)), "");
}

TEST(FixedMpsTest, Comment)
{
    const std::string file = text({
        "NAME",
        "ROWS",
        " N  cost",
        "COLUMNS",
        "    x         cost      1.0            $ a note",
        "ENDATA",
    });
    const std::string free = text({"NAME", "ROWS", " N cost", "COLUMNS", " x cost 1.0", "ENDATA"});
    EXPECT_EQ(difference(read_fixed(file), read_sample(free)), "");
}

TEST(FixedMpsTest, CardNumber)
{
    const std::string file = text({
        "NAME",
        "ROWS",
        " N  cost",
        " L  cap",
        "COLUMNS",
        "    x         cost      1.0            cap       2.0                    00000001",
        "ENDATA",
    });
    EXPECT_EQ(fault_of(file, MpsForm::Fixed),
              bad(6, "'2.0                    00000001' is not a number"));
}

TEST(FixedMpsTest, SpacedMarker)
{
    const std::string file = text({
        "NAME",
        "ROWS",
        " N  cost",
        "COLUMNS",
        "    MARK 1    'MARKER'                 'INTORG'",
        "ENDATA",
    });
    EXPECT_EQ(fault_of(file, MpsForm::Fixed), bad(5, "unknown marker "));
}

TEST(FixedMpsTest, EmptyName)
{
    const std::string file = text({
        "NAME",
        "ROWS",
        " N  cost",
        "COLUMNS",
        "              cost      1.0",
        "ENDATA",
    });
    EXPECT_EQ(fault_of(file, MpsForm::Fixed), bad(5, "name is empty"));
}

TEST(FixedMpsTest, LaterVector)
{
    const std::string file = text({
        "NAME",
        "ROWS",
        " L  cap",
        "COLUMNS",
        "    x         cap       1.0",
        "RHS",
        "              cap       1.0",
        "    rhs       cap       2.0",
        "ENDATA",
    });
    const auto heard = warnings_of(file, MpsForm::Fixed);
    ASSERT_EQ(heard.size(), 1U);
    const std::string ignored = "RHS vector 'rhs' is ignored, only '' is read";
    EXPECT_EQ(heard[0].text, "file 't.mps' line 8: " + ignored);
    EXPECT_EQ(heard[0].line, "8");
}

TEST(FixedMpsTest, FreeRefusals)
{
    const std::string spaced = text({"NAME", "ROWS", " L  cap one", "ENDATA"});
    EXPECT_EQ(fault_of(spaced), bad(3, "a row needs a sense and a name"));
    const std::string blank = text({
        "NAME",
        "ROWS",
        " L  cap",
        "COLUMNS",
        "    x         cap       1.0",
        "RHS",
        "              cap       1.0",
        "ENDATA",
    });
    const std::string fault = "an RHS line needs a vector and one or two pairs of row and value";
    EXPECT_EQ(fault_of(blank), bad(7, fault));
}
