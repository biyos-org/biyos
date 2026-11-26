#include "core/memory/room.hpp"

#include <gtest/gtest.h>

#include <vector>

using biyos::make_room;
using biyos::reserve_more;
using biyos::room;

TEST(RoomTest, Doubles)
{
    std::vector<int> items(4);
    items.shrink_to_fit();
    make_room(items, 5);
    EXPECT_GE(items.capacity(), 8U);
}

TEST(RoomTest, Needed)
{
    std::vector<int> items(4);
    make_room(items, 20);
    EXPECT_GE(items.capacity(), 20U);
}

TEST(RoomTest, Enough)
{
    std::vector<int> items;
    items.reserve(16);
    const auto capacity = items.capacity();
    make_room(items, capacity);
    EXPECT_EQ(items.capacity(), capacity);
}

TEST(RoomTest, Extra)
{
    std::vector<int> items(10);
    reserve_more(items, 30);
    EXPECT_GE(items.capacity(), 40U);
    EXPECT_EQ(items.size(), 10U);
}

TEST(RoomTest, Left)
{
    const std::vector<double> items(3);
    EXPECT_EQ(room(items) + 3, room(std::vector<double>()));
}
