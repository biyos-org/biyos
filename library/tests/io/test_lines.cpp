#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "io/lines.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>
#include <vector>

namespace
{

using biyos::Bits;
using biyos::Error;
using biyos::ErrorKind;
using biyos::Lines;
using biyos::number_of;
using biyos::Real;
using biyos::trimmed;

std::vector<std::string_view> fields_of(const Lines &lines)
{
    return {lines.fields().begin(), lines.fields().end()};
}

void expect_number(std::string_view field, Real wanted)
{
    const std::optional<Real> found = number_of(field);
    ASSERT_TRUE(found.has_value()) << field;
    EXPECT_EQ(std::bit_cast<Bits>(*found), std::bit_cast<Bits>(wanted)) << field;
}

}

TEST(LineTest, Fields)
{
    std::istringstream input(" a\tb  c\r\n");
    Lines lines(input, "a.mps");
    ASSERT_TRUE(lines.next());
    EXPECT_EQ(fields_of(lines), (std::vector<std::string_view>{"a", "b", "c"}));
    EXPECT_EQ(lines.text().front(), ' ');
}

TEST(LineTest, Numbers)
{
    std::istringstream input("NAME x\n\n  \nROWS");
    Lines lines(input, "a.mps");
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

TEST(LineTest, Split)
{
    std::istringstream input(" a    b c d\r\n");
    Lines lines(input, "a.mps");
    ASSERT_TRUE(lines.next());
    const std::size_t starts[] = {1, 3, 6, 9, 20};
    lines.split(starts);
    EXPECT_EQ(fields_of(lines), (std::vector<std::string_view>{"a", "", "b c", "d"}));
    lines.split(std::span(starts).first(1));
    EXPECT_EQ(fields_of(lines), (std::vector<std::string_view>{"a    b c d"}));
}

TEST(LineTest, Trimmed)
{
    EXPECT_EQ(trimmed(" \ta b\r"), "a b");
    EXPECT_EQ(trimmed("ab"), "ab");
    EXPECT_EQ(trimmed(" \t "), "");
    EXPECT_EQ(trimmed(""), "");
}

TEST(LineTest, Where)
{
    std::istringstream input("a\nb\n");
    Lines lines(input, "dir/a.mps");
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
    Lines lines(input, "a.mps");
    try
    {
        lines.next();
        FAIL() << "a bad stream was read";
    }
    catch (const Error &error)
    {
        EXPECT_EQ(error.kind(), ErrorKind::FileAccess);
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
        EXPECT_FALSE(number_of(field).has_value()) << field;
    }
}
