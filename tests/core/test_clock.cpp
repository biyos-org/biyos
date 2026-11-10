#include "core/clock.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace
{

using namespace std::chrono_literals;

class ClockTest : public ::testing::Test
{
  protected:
    biyos::FakeClock clock;
};

}

TEST_F(ClockTest, FakeTime)
{
    EXPECT_EQ(clock.wall_time(), 0ns);
    EXPECT_EQ(clock.cpu_time(), 0ns);
    clock.advance_wall(5ns);
    clock.advance_cpu(3ns);
    clock.advance_wall(2s);
    EXPECT_EQ(clock.wall_time(), 2s + 5ns);
    EXPECT_EQ(clock.cpu_time(), 3ns);
}

TEST_F(ClockTest, SystemTime)
{
    const biyos::SystemClock system;
    const std::chrono::nanoseconds wall = system.wall_time();
    const std::chrono::nanoseconds cpu = system.cpu_time();
    EXPECT_GE(system.wall_time(), wall);
    EXPECT_GE(system.cpu_time(), cpu);
    EXPECT_GE(cpu, 0ns);
}
