#include "core/text/strings.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <utility>

using biyos::Strings;

TEST(StringTest, Keep)
{
    Strings strings;
    std::string text = "replay.txt";
    const std::string_view kept = strings.keep(text);
    text = "other.txt";
    EXPECT_EQ(kept, "replay.txt");
    EXPECT_EQ(kept.data()[kept.size()], '\0');
}

TEST(StringTest, Reuse)
{
    Strings strings;
    const std::string_view first = strings.keep("replay.txt");
    EXPECT_EQ(strings.keep(std::string("replay.txt")).data(), first.data());
    EXPECT_NE(strings.keep("replay").data(), first.data());
    EXPECT_EQ(strings.keep("").data(), strings.keep(std::string()).data());
}

TEST(StringTest, Stable)
{
    Strings strings;
    const std::string_view first = strings.keep("first");
    for (int count = 0; count < 100; ++count)
    {
        strings.keep(std::to_string(count));
    }
    EXPECT_EQ(first, "first");
    EXPECT_EQ(strings.keep("first").data(), first.data());
}

TEST(StringTest, Move)
{
    Strings strings;
    const std::string_view kept = strings.keep("a name too long to sit inside the string itself");
    Strings moved(std::move(strings));
    EXPECT_EQ(moved.keep(kept).data(), kept.data());
    Strings other;
    other = std::move(moved);
    EXPECT_EQ(other.keep(kept).data(), kept.data());
}
