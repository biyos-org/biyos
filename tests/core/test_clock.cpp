#include "core/clock.hpp"
#include "core/deadline.hpp"
#include "core/environment.hpp"
#include "core/events.hpp"
#include "core/options.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace
{

using namespace std::chrono_literals;
using biyos::EventKind;
using biyos::Level;
using biyos::Option;
using Times = std::vector<std::chrono::nanoseconds>;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr std::chrono::nanoseconds kForever = (std::chrono::nanoseconds::max)();

class ClockTest : public ::testing::Test
{
  protected:
    bool reached() const { return biyos::Deadline(options, clock).reached(); }

    biyos::Options options;
    biyos::FakeClock clock;
};

void blank(biyos::Event &) {}

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

TEST_F(ClockTest, WallLimit)
{
    options.set_double(Option::TimeLimit, 1.5);
    clock.advance_wall(4s);
    const biyos::Deadline deadline(options, clock);
    clock.advance_wall(1500ms - 1ns);
    clock.advance_cpu(1h);
    EXPECT_FALSE(deadline.reached());
    clock.advance_wall(1ns);
    EXPECT_TRUE(deadline.reached());
}

TEST_F(ClockTest, CpuLimit)
{
    options.set_double(Option::CpuTimeLimit, 0.25);
    clock.advance_cpu(3s);
    const biyos::Deadline deadline(options, clock);
    clock.advance_cpu(250ms - 1ns);
    clock.advance_wall(1h);
    EXPECT_FALSE(deadline.reached());
    clock.advance_cpu(1ns);
    EXPECT_TRUE(deadline.reached());
}

TEST_F(ClockTest, ZeroLimit)
{
    options.set_double(Option::TimeLimit, 0.0);
    EXPECT_TRUE(reached());
    options.set_double(Option::TimeLimit, kInfinity);
    options.set_double(Option::CpuTimeLimit, 0.0);
    EXPECT_TRUE(reached());
}

TEST_F(ClockTest, InfiniteLimit)
{
    const biyos::Deadline deadline(options, clock);
    clock.advance_wall(kForever);
    clock.advance_cpu(kForever);
    EXPECT_FALSE(deadline.reached());
}

TEST_F(ClockTest, HugeLimit)
{
    options.set_double(Option::TimeLimit, 1e10);
    options.set_double(Option::CpuTimeLimit, 1e300);
    const biyos::Deadline deadline(options, clock);
    clock.advance_wall(kForever);
    clock.advance_cpu(kForever);
    EXPECT_FALSE(deadline.reached());
}

TEST_F(ClockTest, LongLimit)
{
    options.set_double(Option::TimeLimit, 9e9);
    const biyos::Deadline deadline(options, clock);
    clock.advance_wall(std::chrono::nanoseconds(9'000'000'000'000'000'000) - 1ns);
    EXPECT_FALSE(deadline.reached());
    clock.advance_wall(1ns);
    EXPECT_TRUE(deadline.reached());
}

TEST_F(ClockTest, PartialTick)
{
    options.set_double(Option::CpuTimeLimit, 1.5e-9);
    const biyos::Deadline deadline(options, clock);
    clock.advance_cpu(1ns);
    EXPECT_FALSE(deadline.reached());
    clock.advance_cpu(1ns);
    EXPECT_TRUE(deadline.reached());
}

TEST_F(ClockTest, ReadOnce)
{
    options.set_double(Option::TimeLimit, 1.0);
    const biyos::Deadline deadline(options, clock);
    options.set_double(Option::TimeLimit, 0.0);
    options.set_double(Option::CpuTimeLimit, 0.0);
    EXPECT_FALSE(deadline.reached());
    clock.advance_wall(1s);
    EXPECT_TRUE(deadline.reached());
}

TEST_F(ClockTest, EventTime)
{
    auto owned = std::make_unique<biyos::FakeClock>();
    biyos::FakeClock &fake = *owned;
    fake.advance_wall(7s);
    biyos::Environment env(std::move(owned));
    Times times;
    const auto keep = [&times](const biyos::Event &event) { times.push_back(event.time); };
    env.events().subscribe(EventKind::Message, Level::Debug, keep);
    env.events().emit(EventKind::Message, Level::Info, blank);
    fake.advance_wall(250ns);
    fake.advance_cpu(1s);
    env.events().emit(EventKind::Message, Level::Info, blank);
    fake.advance_wall(3ms);
    env.events().emit(EventKind::Message, Level::Error, blank);
    EXPECT_EQ(times, (Times{0ns, 250ns, 3ms + 250ns}));
}
