#include "files.hpp"
#include "loading.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using biyos::test::nl::bad;
using biyos::test::nl::fault_of;
using biyos::test::nl::kDefinedHeader;
using biyos::test::nl::tiny_fault;

namespace
{

constexpr std::string_view kNotTextNlFile = R"(x1 1 1 0
)";

constexpr std::string_view kShortNlFile = R"(g3 1 1 0
 1 1 1 0
)";

constexpr std::string_view kEndsNlFile = R"(g3 1 1 0
 1 1 1 0 0
)";

constexpr std::string_view kNegativeNlFile = R"(g3 1 1 0
 -1 1 1 0 0
)";

constexpr std::string_view kLayoutNlFile = R"(g3 1 1 0
 1 0 0 0 0
 0 0
 0 0
 2 0 0
 0 0 0 1
 0 0 0 0 0
 0 0
 0 0
 0 0 0 0 0
)";

constexpr std::string_view kRoomNlFile = R"(g3 1 1 0
 20000000 0 0 0 0
 0 0
 0 0
 0 0 0
 0 0 0 1
 0 0 0 0 0
 0 0
 0 0
 0 0 0 0 0
)";

constexpr std::string_view kBoundsNlFile = R"(g3 1 1 0
 1 0 0 0 0
 0 0
 0 0
 0 0 0
 0 0 0 1
 1 0 0 0 0
 0 0
 0 0
 0 0 0 0 0
b
0 0 2
)";

constexpr std::string_view kSegmentTail = R"(Q0
)";

constexpr std::string_view kRepeatedTail = R"(b
3
b
3
)";

constexpr std::string_view kMissingTail = R"(O0 0
n0
r
3
b
3
)";

constexpr std::string_view kNoBoundsTail = R"(C0
n0
O0 0
n0
b
3
)";

constexpr std::string_view kRangeTail = R"(C1
n0
)";

constexpr std::string_view kIntegerTail = R"(C0.5
n0
)";

constexpr std::string_view kTwiceTail = R"(C0
n0
C0
n1
)";

constexpr std::string_view kOperatorTail = R"(C0
o99
v0
)";

constexpr std::string_view kLeafTail = R"(C0
q1
)";

constexpr std::string_view kEndsTail = R"(C0
o0
n1
)";

constexpr std::string_view kVariableTail = R"(C0
v3
)";

constexpr std::string_view kEarlyTail = R"(C0
v1
)";

constexpr std::string_view kRedefinedTail = R"(V1 0 0
n1
V1 0 0
n2
)";

constexpr std::string_view kBreakpointsTail = R"(C0
o64
3
n1
n2
n1
n1
n1
v0
)";

constexpr std::string_view kNoOperandsTail = R"(C0
o54
0
)";

constexpr std::string_view kTermTwiceTail = R"(J0 2
0 1
0 2
)";

constexpr std::string_view kBoundCodeTail = R"(b
7 1
)";

constexpr std::string_view kSenseTail = R"(O0 2
n0
)";

constexpr std::string_view kColumnsTail = R"(k1
0
)";

constexpr std::string_view kSuffixKindTail = R"(S9 0 zork
)";

constexpr std::string_view kSuffixIndexTail = R"(S1 1 zork
1 2
)";

constexpr std::string_view kSuffixValueTail = R"(S0 1 zork
0 0.5
)";

constexpr std::string_view kGluedRanges = R"(rjunk
)";

constexpr std::string_view kGluedBounds = R"(bjunk
)";

}

TEST(NlTest, Empty)
{
    EXPECT_EQ(fault_of(""), "bad file: file 't.nl': is empty");
}

TEST(NlTest, NotText)
{
    EXPECT_EQ(fault_of(kNotTextNlFile), bad(1, "the file does not start with 'g'"));
}

TEST(NlTest, ShortHeader)
{
    EXPECT_EQ(fault_of(kShortNlFile), bad(2, "the header line holds fewer than 5 values"));
}

TEST(NlTest, HeaderEnds)
{
    EXPECT_EQ(fault_of(kEndsNlFile), bad(2, "the file ends inside the header"));
}

TEST(NlTest, NegativeCount)
{
    EXPECT_EQ(fault_of(kNegativeNlFile), bad(2, "'-1' is not a count"));
}

TEST(NlTest, Layout)
{
    EXPECT_EQ(fault_of(kLayoutNlFile), bad(10, "the header's variable counts do not add up"));
}

TEST(NlTest, Segment)
{
    EXPECT_EQ(tiny_fault(kSegmentTail), bad(11, "segment 'Q0' is unknown"));
}

TEST(NlTest, Repeated)
{
    EXPECT_EQ(tiny_fault(kRepeatedTail), bad(13, "segment 'b' is repeated"));
}

TEST(NlTest, Missing)
{
    EXPECT_EQ(tiny_fault(kMissingTail), bad(16, "constraint 0 has no expression"));
}

TEST(NlTest, NoBounds)
{
    EXPECT_EQ(tiny_fault(kNoBoundsTail), bad(16, "the file has no constraint bounds"));
}

TEST(NlTest, Range)
{
    EXPECT_EQ(tiny_fault(kRangeTail), bad(11, "constraint 1 is out of range"));
}

TEST(NlTest, Integer)
{
    EXPECT_EQ(tiny_fault(kIntegerTail), bad(11, "'0.5' is not an integer"));
}

TEST(NlTest, Twice)
{
    EXPECT_EQ(tiny_fault(kTwiceTail), bad(13, "constraint 0 is repeated"));
}

TEST(NlTest, Operator)
{
    EXPECT_EQ(tiny_fault(kOperatorTail), bad(12, "operator '99' is unknown"));
}

TEST(NlTest, Leaf)
{
    EXPECT_EQ(tiny_fault(kLeafTail), bad(12, "expected an expression, not 'q1'"));
}

TEST(NlTest, Ends)
{
    EXPECT_EQ(tiny_fault(kEndsTail), bad(13, "the file ends inside an expression"));
}

TEST(NlTest, Variable)
{
    EXPECT_EQ(tiny_fault(kVariableTail), bad(12, "variable 3 is out of range"));
}

TEST(NlTest, Early)
{
    const std::string file = std::string(kDefinedHeader) + std::string(kEarlyTail);
    EXPECT_EQ(fault_of(file), bad(12, "defined variable 1 is used before its definition"));
}

TEST(NlTest, Redefined)
{
    const std::string file = std::string(kDefinedHeader) + std::string(kRedefinedTail);
    EXPECT_EQ(fault_of(file), bad(13, "defined variable 1 is repeated"));
}

TEST(NlTest, Breakpoints)
{
    EXPECT_EQ(tiny_fault(kBreakpointsTail), bad(19, "the breakpoints of a plterm do not increase"));
}

TEST(NlTest, NoOperands)
{
    EXPECT_EQ(tiny_fault(kNoOperandsTail), bad(13, "operator sum has no operands"));
}

TEST(NlTest, TermTwice)
{
    EXPECT_EQ(tiny_fault(kTermTwiceTail), bad(13, "variable 0 is repeated"));
}

TEST(NlTest, BoundCode)
{
    EXPECT_EQ(tiny_fault(kBoundCodeTail), bad(12, "bound code '7' is unknown"));
}

TEST(NlTest, Sense)
{
    EXPECT_EQ(tiny_fault(kSenseTail), bad(11, "objective sense '2' is unknown"));
}

TEST(NlTest, Columns)
{
    EXPECT_EQ(tiny_fault(kColumnsTail), bad(11, "the k segment holds 1 counts, not 0"));
}

TEST(NlTest, Room)
{
    EXPECT_EQ(fault_of(kRoomNlFile), bad(10, "the header counts more records than the file holds"));
}

TEST(NlTest, Glued)
{
    EXPECT_EQ(tiny_fault(kGluedRanges), bad(11, "the line holds 1 values, not 0"));
    EXPECT_EQ(tiny_fault(kGluedBounds), bad(11, "the line holds 1 values, not 0"));
}

TEST(NlTest, Bounds)
{
    EXPECT_EQ(fault_of(kBoundsNlFile),
              bad(12, "variable 0: bounds [0, 2] are outside [0, 1], the widest of its kind"));
}

TEST(NlTest, Suffix)
{
    EXPECT_EQ(tiny_fault(kSuffixKindTail), bad(11, "suffix kind '9' is unknown"));
    EXPECT_EQ(tiny_fault(kSuffixIndexTail), bad(12, "constraint 1 is out of range"));
    EXPECT_EQ(tiny_fault(kSuffixValueTail), bad(12, "'0.5' is not an integer"));
}
