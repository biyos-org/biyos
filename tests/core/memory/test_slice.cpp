#include "core/memory/slice.hpp"

#include "core/numbers.hpp"

#include <gtest/gtest.h>

#include <span>
#include <vector>

TEST(SliceTest, Parts)
{
    std::vector<int> items{1, 2, 3, 4, 5};
    const std::vector<biyos::Offset> starts{0, 2, 2, 5};
    const std::span<int> first = biyos::slice(std::span(items), starts, 0);
    const std::span<int> last = biyos::slice(std::span(items), starts, 2);
    EXPECT_EQ(first.data(), items.data());
    EXPECT_EQ(first.size(), 2U);
    EXPECT_TRUE(biyos::slice(std::span(items), starts, 1).empty());
    EXPECT_EQ(last.data(), items.data() + 2);
    EXPECT_EQ(last.size(), 3U);
}

TEST(SliceTest, Writes)
{
    std::vector<int> items{1, 2, 3};
    const std::vector<biyos::Offset> starts{0, 1, 3};
    biyos::slice(std::span(items), starts, 1)[0] = 7;
    EXPECT_EQ(items[1], 7);
}
