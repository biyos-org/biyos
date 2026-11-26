#include "files.hpp"
#include "loading.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

using biyos::test::nl::fault_of;
using biyos::test::nl::infos_of;
using biyos::test::nl::refused;
using biyos::test::nl::tiny_fault;
using biyos::test::nl::tiny_with;

namespace
{

constexpr std::string_view kBinaryNlFile = R"(b3 1 1 0
)";

constexpr std::string_view kFunctionsTail = R"(F0 1 -1 myfunc
)";

constexpr std::string_view kSuffixesTail = R"(S0 1 zork
0 1
)";

constexpr std::string_view kLogicalTail = R"(L0
n1
)";

constexpr std::string_view kRemainderTail = R"(C0
o4
v0
n2
)";

constexpr std::string_view kAtan2Tail = R"(C0
o48
v0
n2
)";

constexpr std::string_view kLtTail = R"(C0
o22
v0
n2
)";

constexpr std::string_view kIfTail = R"(C0
o35
n1
v0
n2
)";

constexpr std::string_view kFloorTail = R"(C0
o13
v0
)";

constexpr std::string_view kAlldiffTail = R"(C0
o74
2
v0
n2
)";

constexpr std::string_view kCallTail = R"(C0
f0 1
v0
)";

constexpr std::string_view kStringTail = R"(C0
h3:abc
)";

constexpr std::string_view kComplementsTail = R"(r
5 1 0
)";

constexpr std::string_view kSkippedTail = R"(C0
n0
O0 0
n0
d1
0 1
x1
0 2
r
3
b
3
)";

}

TEST(NlTest, Binary)
{
    EXPECT_EQ(fault_of(kBinaryNlFile), refused(1, "binary NL"));
}

TEST(NlTest, Functions)
{
    EXPECT_EQ(tiny_fault(kFunctionsTail), refused(11, "imported functions"));
}

TEST(NlTest, Suffixes)
{
    EXPECT_EQ(tiny_fault(kSuffixesTail), refused(11, "suffixes"));
}

TEST(NlTest, Logical)
{
    EXPECT_EQ(tiny_fault(kLogicalTail), refused(11, "logical constraints"));
}

TEST(NlTest, Remainder)
{
    EXPECT_EQ(tiny_fault(kRemainderTail), refused(12, "operator rem"));
}

TEST(NlTest, Refused)
{
    EXPECT_EQ(tiny_fault(kAtan2Tail), refused(12, "operator atan2"));
    EXPECT_EQ(tiny_fault(kLtTail), refused(12, "operator lt"));
    EXPECT_EQ(tiny_fault(kIfTail), refused(12, "operator if"));
    EXPECT_EQ(tiny_fault(kFloorTail), refused(12, "operator floor"));
    EXPECT_EQ(tiny_fault(kAlldiffTail), refused(12, "operator alldiff"));
}

TEST(NlTest, Call)
{
    EXPECT_EQ(tiny_fault(kCallTail), refused(12, "imported functions"));
}

TEST(NlTest, String)
{
    EXPECT_EQ(tiny_fault(kStringTail), refused(12, "strings"));
}

TEST(NlTest, Complements)
{
    EXPECT_EQ(tiny_fault(kComplementsTail), refused(12, "complementarity constraints"));
}

TEST(NlTest, Skipped)
{
    const std::vector<std::string> wanted{
        "file 't.nl' line 15: the file's dual guesses are skipped",
        "file 't.nl' line 17: the file's starting point is skipped",
    };
    EXPECT_EQ(infos_of(tiny_with(kSkippedTail)), wanted);
}
