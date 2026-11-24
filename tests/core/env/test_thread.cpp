#include "core/thread.hpp"

#include <gtest/gtest.h>

#include <thread>

TEST(ThreadTest, Stable)
{
    EXPECT_EQ(biyos::this_thread(), biyos::this_thread());
}

TEST(ThreadTest, Distinct)
{
    biyos::ThreadId other;
    std::thread worker([&other] { other = biyos::this_thread(); });
    worker.join();
    EXPECT_NE(other, biyos::this_thread());
}
