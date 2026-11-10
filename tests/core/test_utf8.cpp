#include "core/utf8.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string_view>

namespace
{

std::size_t valid_size(std::string_view text)
{
    const biyos::utf8::Sequence sequence = biyos::utf8::first_sequence(text);
    return sequence.valid ? sequence.size : 0;
}

std::size_t subpart(std::string_view text)
{
    const biyos::utf8::Sequence sequence = biyos::utf8::first_sequence(text);
    return sequence.valid ? 0 : sequence.size;
}

}

TEST(Utf8Test, WellFormed)
{
    EXPECT_EQ(valid_size("ab"), 1U);
    EXPECT_EQ(valid_size(std::string_view("\0", 1)), 1U);
    EXPECT_EQ(valid_size("\x7f"), 1U);
    EXPECT_EQ(valid_size("\xc2\x80"), 2U);
    EXPECT_EQ(valid_size("\xdf\xbf"), 2U);
    EXPECT_EQ(valid_size("\xe0\xa0\x80"), 3U);
    EXPECT_EQ(valid_size("\xe1\x80\x80"), 3U);
    EXPECT_EQ(valid_size("\xed\x9f\xbf"), 3U);
    EXPECT_EQ(valid_size("\xee\x80\x80"), 3U);
    EXPECT_EQ(valid_size("\xef\xbf\xbf"), 3U);
    EXPECT_EQ(valid_size("\xf0\x90\x80\x80"), 4U);
    EXPECT_EQ(valid_size("\xf3\xbf\xbf\xbf"), 4U);
    EXPECT_EQ(valid_size("\xf4\x8f\xbf\xbf "), 4U);
}

TEST(Utf8Test, IllFormed)
{
    EXPECT_EQ(subpart("\x80"), 1U);
    EXPECT_EQ(subpart("\xbf\xbf"), 1U);
    EXPECT_EQ(subpart("\xc0\xaf"), 1U);
    EXPECT_EQ(subpart("\xc1\xbf"), 1U);
    EXPECT_EQ(subpart("\xc3"), 1U);
    EXPECT_EQ(subpart("\xc3 "), 1U);
    EXPECT_EQ(subpart("\xe0\x9f\xbf"), 1U);
    EXPECT_EQ(subpart("\xed\xa0\x80"), 1U);
    EXPECT_EQ(subpart("\xf0\x8f\xbf\xbf"), 1U);
    EXPECT_EQ(subpart("\xf4\x90\x80\x80"), 1U);
    EXPECT_EQ(subpart("\xf5\x80\x80\x80"), 1U);
    EXPECT_EQ(subpart("\xff"), 1U);
    EXPECT_EQ(subpart("\xe2\x82"), 2U);
    EXPECT_EQ(subpart("\xe2\x82 "), 2U);
    EXPECT_EQ(subpart("\xf0\x9d\x84"), 3U);
    EXPECT_EQ(subpart("\xf0\x9d\x84\xc3"), 3U);
}
