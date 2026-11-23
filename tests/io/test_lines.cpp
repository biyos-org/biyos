#include "core/error.hpp"
#include "io/lines.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

namespace
{

std::vector<std::string_view> fields_of(const biyos::Lines &lines)
{
    return {lines.fields().begin(), lines.fields().end()};
}

void expect_number(std::string_view field, double wanted)
{
    const std::optional<double> found = biyos::number_of(field);
    ASSERT_TRUE(found.has_value()) << field;
    EXPECT_EQ(std::bit_cast<std::uint64_t>(*found), std::bit_cast<std::uint64_t>(wanted)) << field;
}

}

TEST(LineTest, Fields)
{
    std::istringstream input(" a\tb  c\r\n");
    biyos::Lines lines(input, "a.mps");
    ASSERT_TRUE(lines.next());
    EXPECT_EQ(fields_of(lines), (std::vector<std::string_view>{"a", "b", "c"}));
    EXPECT_EQ(lines.text().front(), ' ');
}

TEST(LineTest, Numbers)
{
    std::istringstream input("NAME x\n\n  \nROWS");
    biyos::Lines lines(input, "a.mps");
    ASSERT_TRUE(lines.next());
    EXPECT_EQ(lines.number(), 1U);
    ASSERT_TRUE(lines.next());
    EXPECT_TRUE(lines.fields().empty());
    ASSERT_TRUE(lines.next());
    EXPECT_TRUE(lines.fields().empty());
    ASSERT_TRUE(lines.next());
    EXPECT_EQ(lines.number(), 4U);
    EXPECT_EQ(fields_of(lines), (std::vector<std::string_view>{"ROWS"}));
    EXPECT_FALSE(lines.next());
    EXPECT_TRUE(lines.fields().empty());
}

TEST(LineTest, Where)
{
    std::istringstream input("a\nb\n");
    biyos::Lines lines(input, "dir/a.mps");
    EXPECT_EQ(lines.where(), "file 'dir/a.mps'");
    lines.next();
    lines.next();
    EXPECT_EQ(lines.where(), "file 'dir/a.mps' line 2");
    EXPECT_EQ(lines.file(), "dir/a.mps");
}

TEST(LineTest, Unreadable)
{
    std::istringstream input("a\n");
    input.setstate(std::ios::badbit);
    biyos::Lines lines(input, "a.mps");
    try
    {
        lines.next();
        FAIL() << "a bad stream was read";
    }
    catch (const biyos::Error &error)
    {
        EXPECT_EQ(error.kind(), biyos::ErrorKind::FileAccess);
        EXPECT_EQ(error.object(), "file 'a.mps'");
    }
}

TEST(NumberTest, Accepted)
{
    expect_number("1", 1.0);
    expect_number("-2.5", -2.5);
    expect_number("+3", 3.0);
    expect_number(".5", 0.5);
    expect_number("-.5", -0.5);
    expect_number("5.", 5.0);
    expect_number("1e3", 1000.0);
    expect_number("1E-2", 0.01);
}

TEST(NumberTest, Rejected)
{
    for (const std::string_view field :
         {"",    "+",  "-",   ".",    "inf", "-inf", "infinity", "nan",   "+-1",    "-+1",
          "--1", "1e", "1e+", "0x10", "1,5", "1 ",   "e5",       "1e400", "-1e400", "2.5d+0"})
    {
        EXPECT_FALSE(biyos::number_of(field).has_value()) << field;
    }
}
