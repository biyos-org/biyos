#include "core/thread.hpp"

#include <gtest/gtest.h>

#include <thread>

using biyos::this_thread;
using biyos::ThreadId;

TEST(ThreadTest, Stable)
{
    EXPECT_EQ(this_thread(), this_thread());
}

TEST(ThreadTest, Distinct)
{
    ThreadId other;
    std::thread worker([&other] { other = this_thread(); });
    worker.join();
    EXPECT_NE(other, this_thread());
}
