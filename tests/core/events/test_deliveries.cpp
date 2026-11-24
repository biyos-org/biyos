#include "core/deliveries.hpp"
#include "core/depth.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <functional>
#include <latch>
#include <thread>
#include <vector>

namespace
{

using biyos::Deliveries;
using biyos::Depth;

constexpr std::size_t kThreads = 4;

bool apart(const Deliveries &deliveries)
{
    bool seen = true;
    std::thread([&] { seen = deliveries.delivering() || deliveries.filling(); }).join();
    return seen;
}

void hold(Deliveries &deliveries, std::latch &linked, std::latch &left, bool &kept)
{
    const Deliveries::Entry entry(deliveries);
    const Depth inside(entry.state().depth);
    linked.count_down();
    left.wait();
    kept = deliveries.delivering();
}

std::thread beside(Deliveries &deliveries, std::latch &linked, std::latch &left, bool &kept)
{
    const Deliveries::Entry entry(deliveries);
    std::thread holder(hold,
                       std::ref(deliveries),
                       std::ref(linked),
                       std::ref(left),
                       std::ref(kept));
    linked.wait();
    return holder;
}

void own(Deliveries &deliveries, std::latch &linked, std::latch &checked, bool &found)
{
    const Deliveries::Entry entry(deliveries);
    const Depth inside(entry.state().depth);
    linked.arrive_and_wait();
    found = deliveries.delivery() == &entry.state();
    checked.arrive_and_wait();
}

}

TEST(DeliveryTest, Idle)
{
    Deliveries deliveries;
    EXPECT_FALSE(deliveries.delivering());
    EXPECT_FALSE(deliveries.filling());
    EXPECT_EQ(deliveries.delivery(), nullptr);
}

TEST(DeliveryTest, Depth)
{
    Deliveries deliveries;
    const Deliveries::Entry entry(deliveries);
    EXPECT_FALSE(deliveries.delivering());
    EXPECT_EQ(deliveries.delivery(), nullptr);
    const Depth inside(entry.state().depth);
    EXPECT_TRUE(deliveries.delivering());
    EXPECT_FALSE(deliveries.filling());
    EXPECT_EQ(deliveries.delivery(), &entry.state());
    EXPECT_EQ(entry.state().scope, nullptr);
}

TEST(DeliveryTest, Fills)
{
    Deliveries deliveries;
    const Deliveries::Entry entry(deliveries);
    const Depth inside(entry.state().fills);
    EXPECT_TRUE(deliveries.filling());
    EXPECT_FALSE(deliveries.delivering());
}

TEST(DeliveryTest, Nested)
{
    Deliveries deliveries;
    const Deliveries::Entry outer(deliveries);
    const Depth inside(outer.state().depth);
    {
        const Deliveries::Entry inner(deliveries);
        EXPECT_EQ(&inner.state(), &outer.state());
    }
    EXPECT_TRUE(deliveries.delivering());
}

TEST(DeliveryTest, OtherThread)
{
    Deliveries deliveries;
    const Deliveries::Entry entry(deliveries);
    const Depth delivered(entry.state().depth);
    const Depth filled(entry.state().fills);
    EXPECT_FALSE(apart(deliveries));
    EXPECT_TRUE(deliveries.delivering());
}

TEST(DeliveryTest, Unlink)
{
    Deliveries deliveries;
    std::latch linked(1);
    std::latch left(1);
    bool kept = false;
    std::thread holder = beside(deliveries, linked, left, kept);
    EXPECT_FALSE(deliveries.delivering());
    left.count_down();
    holder.join();
    EXPECT_TRUE(kept);
    EXPECT_FALSE(deliveries.delivering());
}

TEST(DeliveryTest, Threads)
{
    Deliveries deliveries;
    std::latch linked(kThreads);
    std::latch checked(kThreads);
    std::array<bool, kThreads> found{};
    std::vector<std::thread> threads;
    for (bool &mine : found)
    {
        threads.emplace_back(own,
                             std::ref(deliveries),
                             std::ref(linked),
                             std::ref(checked),
                             std::ref(mine));
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(found, (std::array<bool, kThreads>{true, true, true, true}));
    EXPECT_FALSE(deliveries.delivering());
}
