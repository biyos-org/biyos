#include "core/env.hpp"
#include "core/events/events.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "core/options/settings.hpp"
#include "core/status.hpp"
#include "core/timing/clock.hpp"
#include "core/timing/deadline.hpp"
#include "core/timing/interrupt.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace
{

using namespace std::chrono_literals;
using biyos::Clock;
using biyos::Deadline;
using biyos::Env;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::FakeClock;
using biyos::Interrupt;
using biyos::Lookup;
using biyos::Option;
using biyos::Options;
using biyos::Real;
using biyos::Settings;
using biyos::SystemClock;
using biyos::TerminationStatus;
using Times = std::vector<std::chrono::nanoseconds>;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr std::chrono::nanoseconds kForever = (std::chrono::nanoseconds::max)();
constexpr Real kEdgeSeconds = 0x1.12e0be826d695p+33;
constexpr std::chrono::nanoseconds kSpin = 50ms;

class ClockTest : public ::testing::Test
{
  protected:
    bool reached() const { return Deadline(lookup, clock, interrupt).reached(); }

    FakeClock clock;
    Env host;
    Options options{host};
    Lookup lookup{options};
    Interrupt interrupt;
};

class CountingClock final : public Clock
{
  public:
    std::chrono::nanoseconds wall_time() const noexcept override
    {
        ++wall_reads_;
        return 0ns;
    }

    std::chrono::nanoseconds cpu_time() const noexcept override
    {
        ++cpu_reads_;
        return 0ns;
    }

    int wall_reads() const noexcept { return wall_reads_; }
    int cpu_reads() const noexcept { return cpu_reads_; }

  private:
    mutable int wall_reads_ = 0;
    mutable int cpu_reads_ = 0;
};

void blank(Event &) {}

void spin(const Clock &clock)
{
    const std::chrono::nanoseconds start = clock.cpu_time();
    while (clock.cpu_time() - start < kSpin)
    {
    }
}

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
    const SystemClock system;
    const std::chrono::nanoseconds wall = system.wall_time();
    const std::chrono::nanoseconds cpu = system.cpu_time();
    EXPECT_GE(system.wall_time(), wall);
    EXPECT_GE(system.cpu_time(), cpu);
    EXPECT_GE(cpu, 0ns);
}

TEST_F(ClockTest, OtherThread)
{
    const SystemClock system;
    const std::chrono::nanoseconds before = system.cpu_time();
    std::thread([&system] { spin(system); }).join();
    EXPECT_LT(system.cpu_time() - before, kSpin);
}

TEST_F(ClockTest, WallLimit)
{
    options.set_double(Option::TimeLimit, 1.5);
    clock.advance_wall(4s);
    const Deadline deadline(lookup, clock, interrupt);
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
    const Deadline deadline(lookup, clock, interrupt);
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
    const Deadline deadline(lookup, clock, interrupt);
    clock.advance_wall(kForever);
    clock.advance_cpu(kForever);
    EXPECT_FALSE(deadline.reached());
}

TEST_F(ClockTest, AboveBound)
{
    options.set_double(Option::TimeLimit, 1e10);
    options.set_double(Option::CpuTimeLimit, 1e300);
    const Deadline deadline(lookup, clock, interrupt);
    clock.advance_wall(kForever);
    clock.advance_cpu(kForever);
    EXPECT_FALSE(deadline.reached());
}

TEST_F(ClockTest, AtBound)
{
    options.set_double(Option::TimeLimit, kEdgeSeconds);
    options.set_double(Option::CpuTimeLimit, kEdgeSeconds);
    const Deadline deadline(lookup, clock, interrupt);
    clock.advance_wall(kForever);
    clock.advance_cpu(kForever);
    EXPECT_FALSE(deadline.reached());
}

TEST_F(ClockTest, JustBelow)
{
    options.set_double(Option::TimeLimit, std::nextafter(kEdgeSeconds, 0.0));
    const Deadline deadline(lookup, clock, interrupt);
    clock.advance_wall(std::chrono::nanoseconds(9'223'372'036'854'774'784) - 1ns);
    EXPECT_FALSE(deadline.reached());
    clock.advance_wall(1ns);
    EXPECT_TRUE(deadline.reached());
}

TEST_F(ClockTest, BelowBound)
{
    options.set_double(Option::TimeLimit, 9e9);
    const Deadline deadline(lookup, clock, interrupt);
    clock.advance_wall(std::chrono::nanoseconds(9'000'000'000'000'000'000) - 1ns);
    EXPECT_FALSE(deadline.reached());
    clock.advance_wall(1ns);
    EXPECT_TRUE(deadline.reached());
}

TEST_F(ClockTest, PartialTick)
{
    options.set_double(Option::CpuTimeLimit, 1.2e-9);
    const Deadline deadline(lookup, clock, interrupt);
    clock.advance_cpu(1ns);
    EXPECT_FALSE(deadline.reached());
    clock.advance_cpu(1ns);
    EXPECT_TRUE(deadline.reached());
}

TEST_F(ClockTest, TinyLimit)
{
    options.set_double(Option::TimeLimit, std::numeric_limits<Real>::denorm_min());
    const Deadline deadline(lookup, clock, interrupt);
    EXPECT_FALSE(deadline.reached());
    clock.advance_wall(1ns);
    EXPECT_TRUE(deadline.reached());
}

TEST_F(ClockTest, NoReads)
{
    const CountingClock counter;
    const Deadline deadline(lookup, counter, interrupt);
    EXPECT_FALSE(deadline.reached());
    EXPECT_EQ(counter.wall_reads(), 0);
    EXPECT_EQ(counter.cpu_reads(), 0);
}

TEST_F(ClockTest, WallReads)
{
    options.set_double(Option::TimeLimit, 1.0);
    const CountingClock counter;
    const Deadline deadline(lookup, counter, interrupt);
    EXPECT_FALSE(deadline.reached());
    EXPECT_EQ(counter.wall_reads(), 2);
    EXPECT_EQ(counter.cpu_reads(), 0);
}

TEST_F(ClockTest, CpuReads)
{
    options.set_double(Option::CpuTimeLimit, 1.0);
    const CountingClock counter;
    const Deadline deadline(lookup, counter, interrupt);
    EXPECT_FALSE(deadline.reached());
    EXPECT_EQ(counter.wall_reads(), 0);
    EXPECT_EQ(counter.cpu_reads(), 2);
}

TEST_F(ClockTest, ReadOnce)
{
    options.set_double(Option::TimeLimit, 1.0);
    const Deadline deadline(lookup, clock, interrupt);
    options.set_double(Option::TimeLimit, 0.0);
    options.set_double(Option::CpuTimeLimit, 0.0);
    EXPECT_FALSE(deadline.reached());
    clock.advance_wall(1s);
    EXPECT_TRUE(deadline.reached());
}

TEST_F(ClockTest, Interrupted)
{
    const Deadline deadline(lookup, clock, interrupt);
    EXPECT_EQ(deadline.termination(), TerminationStatus::NotSolved);
    interrupt.request();
    EXPECT_TRUE(deadline.reached());
    EXPECT_EQ(deadline.termination(), TerminationStatus::UserStop);
    EXPECT_FALSE(reached());
}

TEST_F(ClockTest, Terminations)
{
    options.set_double(Option::TimeLimit, 2.0);
    options.set_double(Option::CpuTimeLimit, 1.0);
    const Deadline deadline(lookup, clock, interrupt);
    EXPECT_EQ(deadline.termination(), TerminationStatus::NotSolved);
    clock.advance_cpu(1s);
    EXPECT_EQ(deadline.termination(), TerminationStatus::CpuTimeLimit);
    clock.advance_wall(2s);
    EXPECT_EQ(deadline.termination(), TerminationStatus::TimeLimit);
    interrupt.request();
    EXPECT_EQ(deadline.termination(), TerminationStatus::UserStop);
}

TEST_F(ClockTest, StopReads)
{
    options.set_double(Option::TimeLimit, 1.0);
    options.set_double(Option::CpuTimeLimit, 1.0);
    const CountingClock counter;
    const Deadline deadline(lookup, counter, interrupt);
    interrupt.request();
    EXPECT_EQ(deadline.termination(), TerminationStatus::UserStop);
    EXPECT_EQ(counter.wall_reads(), 1);
    EXPECT_EQ(counter.cpu_reads(), 1);
}

TEST_F(ClockTest, UnwantedEvent)
{
    auto owned = std::make_unique<CountingClock>();
    const CountingClock &counter = *owned;
    Env env(std::move(owned));
    env.stream().emit(EventKind::Message, EventLevel::Error, blank);
    env.events().add_hook(EventKind::Message, EventLevel::Error, [](const Event &) {});
    env.stream().emit(EventKind::Message, EventLevel::Warning, blank);
    EXPECT_EQ(counter.wall_reads(), 1);
    env.stream().emit(EventKind::Message, EventLevel::Error, blank);
    EXPECT_EQ(counter.wall_reads(), 2);
    EXPECT_EQ(counter.cpu_reads(), 0);
}

TEST_F(ClockTest, EventTime)
{
    auto owned = std::make_unique<FakeClock>();
    FakeClock &fake = *owned;
    fake.advance_wall(7s);
    Env env(std::move(owned));
    Times times;
    const auto keep = [&times](const Event &event) { times.push_back(event.time); };
    env.events().add_hook(EventKind::Message, EventLevel::Debug, keep);
    env.stream().emit(EventKind::Message, EventLevel::Info, blank);
    fake.advance_wall(250ns);
    fake.advance_cpu(1s);
    env.stream().emit(EventKind::Message, EventLevel::Info, blank);
    fake.advance_wall(3ms);
    env.stream().emit(EventKind::Message, EventLevel::Error, blank);
    EXPECT_EQ(times, (Times{0ns, 250ns, 3ms + 250ns}));
}

TEST_F(ClockTest, ModelLimit)
{
    Settings settings;
    settings.set_double(Option::TimeLimit, 1.0, host.stream());
    settings.set_double(Option::CpuTimeLimit, 0.5, host.stream());
    const Lookup model(settings, options);
    const Deadline own(model, clock, interrupt);
    const Deadline shared(lookup, clock, interrupt);
    clock.advance_cpu(500ms);
    EXPECT_EQ(own.termination(), TerminationStatus::CpuTimeLimit);
    EXPECT_FALSE(shared.reached());
    clock.advance_wall(1s);
    EXPECT_EQ(own.termination(), TerminationStatus::TimeLimit);
    EXPECT_FALSE(shared.reached());
}
