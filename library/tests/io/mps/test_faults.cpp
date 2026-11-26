#include "reading.hpp"

#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

using biyos::test::fault_of;
using biyos::test::Warning;
using biyos::test::warnings_of;

namespace
{

std::string text(std::initializer_list<std::string_view> lines)
{
    std::string joined;
    for (const std::string_view line : lines)
    {
        joined.append(line).append("\n");
    }
    return joined;
}

std::string bad(int line, std::string_view message)
{
    return "bad file: file 't.mps' line " + std::to_string(line) + ": " + std::string(message);
}

std::string with_bound(std::string_view bound)
{
    return text({"NAME t", "ROWS", " N f", "COLUMNS", " x f 1", "RHS", "BOUNDS", bound, "ENDATA"});
}

std::vector<std::string> texts_of(const std::vector<Warning> &warnings)
{
    std::vector<std::string> texts;
    for (const Warning &warning : warnings)
    {
        texts.push_back(warning.line + ": " + warning.text);
    }
    return texts;
}

}

TEST(MpsTest, UnknownRow)
{
    EXPECT_EQ(fault_of(text({"NAME t", "ROWS", " N f", "COLUMNS", " x g 1", "ENDATA"})),
              bad(5, "unknown row 'g'"));
}

TEST(MpsTest, BadNumber)
{
    EXPECT_EQ(fault_of(text({"NAME t", "ROWS", " N f", "COLUMNS", " x f 1x", "ENDATA"})),
              bad(5, "'1x' is not a number"));
}

TEST(MpsTest, TwiceDeclared)
{
    EXPECT_EQ(fault_of(text({"NAME t", "ROWS", " N f", " L f", "COLUMNS", "ENDATA"})),
              bad(4, "row 'f' is declared twice"));
}

TEST(MpsTest, NoEnd)
{
    EXPECT_EQ(fault_of(text({"NAME t", "ROWS", " N f", "COLUMNS", " x f 1"})),
              bad(5, "the file ends without ENDATA"));
}

TEST(MpsTest, OutOfOrder)
{
    EXPECT_EQ(fault_of(text({"NAME t", "COLUMNS", "ROWS", "ENDATA"})),
              bad(2, "section COLUMNS comes before ROWS"));
    EXPECT_EQ(fault_of(text({"NAME t", "ROWS", "COLUMNS", "RHS", "ROWS", "ENDATA"})),
              bad(5, "section ROWS is out of order"));
}

TEST(MpsTest, NoName)
{
    EXPECT_EQ(fault_of(text({"ROWS", "ENDATA"})), bad(1, "section ROWS comes before NAME"));
}

TEST(MpsTest, Contiguous)
{
    EXPECT_EQ(fault_of(text(
                  {"NAME t", "ROWS", " N f", "COLUMNS", " x f 1", " y f 1", " x f 2", "ENDATA"})),
              bad(7, "column 'x' is not contiguous"));
}

TEST(MpsTest, RepeatedEntry)
{
    EXPECT_EQ(fault_of(text({"NAME t", "ROWS", " N f", "COLUMNS", " x f 1 f 2", "ENDATA"})),
              bad(5, "column 'x' has row 'f' twice"));
}

TEST(MpsTest, OpenMarker)
{
    EXPECT_EQ(fault_of(text({"NAME t",
                             "ROWS",
                             " N f",
                             "COLUMNS",
                             " m 'MARKER' 'INTORG'",
                             " x f 1",
                             "RHS",
                             "ENDATA"})),
              bad(7, "an INTORG marker has no INTEND"));
}

TEST(MpsTest, CrossedMarker)
{
    EXPECT_EQ(fault_of(text({"NAME t",
                             "ROWS",
                             " N f",
                             " L c",
                             "COLUMNS",
                             " x f 1",
                             " m 'MARKER' 'INTORG'",
                             " x c 1",
                             " m 'MARKER' 'INTEND'",
                             "ENDATA"})),
              bad(8, "column 'x' crosses a marker"));
}

TEST(MpsTest, Empty)
{
    EXPECT_EQ(fault_of(""), "bad file: file 't.mps': is empty");
}

TEST(MpsTest, SecondRhs)
{
    EXPECT_EQ(fault_of(text(
                  {"NAME t", "ROWS", " L c", "COLUMNS", " x c 1", "RHS", " b c 1 c 2", "ENDATA"})),
              bad(7, "row 'c' has a second right-hand side"));
}

TEST(MpsTest, BoundFaults)
{
    EXPECT_EQ(fault_of(with_bound(" XX b x 1")), bad(8, "unknown bound type 'XX'"));
    EXPECT_EQ(fault_of(with_bound(" UP b x")), bad(8, "UP bound needs a value"));
    EXPECT_EQ(fault_of(with_bound(" LI b x 1.5")),
              bad(8, "LI bound value '1.5' is not an integer"));
    EXPECT_EQ(fault_of(with_bound(" BV b x 2")), bad(8, "BV bound value '2' is not 1"));
    EXPECT_EQ(fault_of(with_bound(" UP b y 1")), bad(8, "unknown column 'y'"));
}

TEST(MpsTest, SecondBound)
{
    EXPECT_EQ(fault_of(text({"NAME t",
                             "ROWS",
                             " N f",
                             "COLUMNS",
                             " x f 1",
                             "RHS",
                             "BOUNDS",
                             " UP b x 1",
                             " FX b x 2",
                             "ENDATA"})),
              bad(9, "column 'x' has a second upper bound"));
}

TEST(MpsTest, AfterEnd)
{
    EXPECT_EQ(fault_of(text({"NAME t", "ROWS", "COLUMNS", "RHS", "ENDATA", "ROWS"})),
              bad(6, "a line follows ENDATA"));
}

TEST(MpsTest, Unsupported)
{
    EXPECT_EQ(
        fault_of(text(
            {"NAME t", "ROWS", " N f", "COLUMNS", " x f 1", "RHS", "QUADOBJ", " x x 1", "ENDATA"})),
        "unsupported: file 't.mps' line 7: section QUADOBJ");
}

TEST(MpsTest, NegativeUpper)
{
    const std::vector<Warning> heard = warnings_of(with_bound(" UP b x -1"));
    ASSERT_EQ(heard.size(), 1U);
    EXPECT_EQ(heard[0].file, "t.mps");
    EXPECT_EQ(heard[0].line, "8");
    EXPECT_EQ(heard[0].text.find("file 't.mps' line 8: "), 0U);
}

TEST(MpsTest, Oddities)
{
    const std::vector<std::string> heard = texts_of(warnings_of(text({
        "NAME t",
        "ROWS",
        " N f",
        " L c",
        "COLUMNS",
        " x f 1 c 1",
        "RHS",
        " a c 1",
        " b c 2",
        " b c 3",
        "RANGES",
        " r f 1",
        "BOUNDS",
        " FR v x 4",
        "ENDATA",
    })));
    const std::vector<std::string> wanted{
        "9: file 't.mps' line 9: RHS vector 'b' is ignored, only 'a' is read",
        "12: file 't.mps' line 12: the range of N row 'f' is ignored",
        "14: file 't.mps' line 14: FR bound takes no value, '4' is ignored",
    };
    EXPECT_EQ(heard, wanted);
}

TEST(MpsTest, NoRhs)
{
    const std::vector<std::string> heard =
        texts_of(warnings_of(text({"NAME t", "ROWS", " N f", "COLUMNS", " x f 1", "ENDATA"})));
    EXPECT_EQ(
        heard,
        std::vector<std::string>{
            "6: file 't.mps' line 6: the file has no RHS section, so every right-hand side is 0"});
}
