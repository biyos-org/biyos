#include "core/env/env.hpp"
#include "core/env/error.hpp"
#include "core/events/events.hpp"
#include "core/events/stream.hpp"
#include "core/timing/clock.hpp"
#include "core/timing/timer.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

using namespace std::chrono_literals;
using biyos::Clock;
using biyos::EntryTimer;
using biyos::Env;
using biyos::Error;
using biyos::ErrorKind;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::FakeClock;
using biyos::Field;
using biyos::Timer;
using biyos::TimingTree;
using Texts = std::vector<std::string>;

class FlakyClock final : public Clock
{
  public:
    std::chrono::nanoseconds wall_time() const noexcept override { return wall; }
    std::chrono::nanoseconds cpu_time() const override;

    std::chrono::nanoseconds wall{0};
    std::chrono::nanoseconds cpu{0};
    bool broken = false;
};

std::chrono::nanoseconds FlakyClock::cpu_time() const
{
    if (broken)
    {
        throw Error(ErrorKind::Internal, "clock", "cannot read the CPU time");
    }
    return cpu;
}

std::string path_of(const TimingTree &tree, std::size_t node)
{
    const TimingTree::Node &at = tree.nodes()[node];
    if (at.parent == TimingTree::kNone)
    {
        return std::string(at.name);
    }
    return path_of(tree, at.parent) + "/" + std::string(at.name);
}

std::string row_of(const TimingTree &tree, std::size_t node)
{
    const TimingTree::Node &at = tree.nodes()[node];
    const std::string count = " count=" + std::to_string(at.count);
    const std::string wall = " wall=" + std::to_string(at.wall.count());
    const std::string cpu = " cpu=" + std::to_string(at.cpu.count());
    return path_of(tree, node) + count + wall + cpu;
}

Texts summary(const TimingTree &tree)
{
    Texts rows;
    for (std::size_t node = 0; node < tree.nodes().size(); ++node)
    {
        rows.push_back(row_of(tree, node));
    }
    return rows;
}

std::string line_of(const Event &event)
{
    std::string line = event.text;
    for (const Field &field : event.fields)
    {
        line += " " + field.name + "=" + field.value;
    }
    return line;
}

Texts lines(const std::vector<Event> &events)
{
    Texts lines;
    for (const Event &event : events)
    {
        lines.push_back(line_of(event));
    }
    return lines;
}

void run(Env &env, TimingTree &tree)
{
    const Timer solve(env.stream(), tree, "nlp", "solve");
    for (int step = 0; step < 3; ++step)
    {
        const Timer iterate(env.stream(), tree, "nlp", "iterate");
    }
}

void abandon(Env &env, TimingTree &tree, FakeClock &clock)
{
    const Timer solve(env.stream(), tree, "nlp", "solve");
    clock.advance_wall(3ns);
    throw std::runtime_error("stop");
}

class TimerTest : public ::testing::Test
{
  protected:
    TimerTest() : TimerTest(std::make_unique<FakeClock>()) {}

    void advance(std::chrono::nanoseconds wall, std::chrono::nanoseconds cpu)
    {
        clock.advance_wall(wall);
        clock.advance_cpu(cpu);
    }

    void lag(const Event &event)
    {
        kept.push_back(event);
        advance(10ns, 10ns);
    }

    void listen(Env &target)
    {
        const auto keep = [this](const Event &event) { kept.push_back(event); };
        target.events().add_hook(EventKind::Span, EventLevel::Debug, keep);
    }

    FakeClock &clock;
    Env env;
    TimingTree tree;
    std::vector<Event> kept;

  private:
    explicit TimerTest(std::unique_ptr<FakeClock> owned) : clock(*owned), env(std::move(owned)) {}
};

}

TEST_F(TimerTest, NestedScopes)
{
    {
        const Timer solve(env.stream(), tree, "nlp", "solve");
        advance(1ns, 1ns);
        {
            const Timer presolve(env.stream(), tree, "nlp", "presolve");
            advance(2ns, 1ns);
            const Timer factor(env.stream(), tree, "nlp", "factor");
            advance(4ns, 2ns);
        }
        {
            const Timer root(env.stream(), tree, "nlp", "root");
            const Timer factor(env.stream(), tree, "nlp", "factor");
            advance(8ns, 4ns);
        }
    }
    {
        const Timer postsolve(env.stream(), tree, "nlp", "postsolve");
        advance(16ns, 8ns);
    }
    const Texts expected{
        "solve count=1 wall=15 cpu=8",
        "solve/presolve count=1 wall=6 cpu=3",
        "solve/presolve/factor count=1 wall=4 cpu=2",
        "solve/root count=1 wall=8 cpu=4",
        "solve/root/factor count=1 wall=8 cpu=4",
        "postsolve count=1 wall=16 cpu=8",
    };
    EXPECT_EQ(summary(tree), expected);
}

TEST_F(TimerTest, RepeatedScope)
{
    for (int pass = 0; pass < 2; ++pass)
    {
        const Timer solve(env.stream(), tree, "nlp", "solve");
        advance(1ns, 1ns);
        for (int step = 0; step < 3; ++step)
        {
            const Timer iterate(env.stream(), tree, "nlp", "iterate");
            advance(2ns, 1ns);
        }
    }
    const Texts expected{
        "solve count=2 wall=14 cpu=8",
        "solve/iterate count=6 wall=12 cpu=6",
    };
    EXPECT_EQ(summary(tree), expected);
}

TEST_F(TimerTest, PhaseEvents)
{
    auto owned = std::make_unique<FakeClock>();
    FakeClock &fake = *owned;
    fake.advance_wall(7s);
    fake.advance_cpu(3ns);
    Env local(std::move(owned));
    listen(local);
    {
        fake.advance_wall(5ns);
        const Timer solve(local.stream(), tree, "nlp", "solve");
        fake.advance_wall(3ns);
        fake.advance_cpu(2ns);
        const Timer presolve(local.stream(), tree, "nlp", "presolve");
        fake.advance_wall(4ns);
        fake.advance_cpu(1ns);
    }
    const Texts expected{
        "solve edge=begin wall=5 cpu=3",
        "presolve edge=begin wall=8 cpu=5",
        "presolve edge=end wall=12 cpu=6",
        "solve edge=end wall=12 cpu=6",
    };
    EXPECT_EQ(lines(kept), expected);
    for (const Event &event : kept)
    {
        EXPECT_EQ(event.kind, EventKind::Span);
        EXPECT_EQ(event.level, EventLevel::Info);
        EXPECT_EQ(event.module, "nlp");
    }
    EXPECT_EQ(kept.back().time, 12ns);
}

TEST_F(TimerTest, SlowHook)
{
    const auto slow = [this](const Event &event) { lag(event); };
    env.events().add_hook(EventKind::Span, EventLevel::Debug, slow);
    {
        const Timer solve(env.stream(), tree, "nlp", "solve");
        advance(1ns, 1ns);
        const Timer presolve(env.stream(), tree, "nlp", "presolve");
        advance(2ns, 1ns);
    }
    const Texts nodes{
        "solve count=1 wall=3 cpu=2",
        "solve/presolve count=1 wall=2 cpu=1",
    };
    EXPECT_EQ(summary(tree), nodes);
    const Texts stamps{
        "solve edge=begin wall=0 cpu=0",
        "presolve edge=begin wall=11 cpu=11",
        "presolve edge=end wall=23 cpu=22",
        "solve edge=end wall=33 cpu=32",
    };
    EXPECT_EQ(lines(kept), stamps);
}

TEST_F(TimerTest, Unwinding)
{
    EXPECT_THROW(abandon(env, tree, clock), std::runtime_error);
    {
        const Timer postsolve(env.stream(), tree, "nlp", "postsolve");
    }
    const Texts expected{
        "solve count=1 wall=3 cpu=0",
        "postsolve count=1 wall=0 cpu=0",
    };
    EXPECT_EQ(summary(tree), expected);
}

TEST_F(TimerTest, OutOfOrder)
{
    std::unique_ptr<Timer> a(new Timer(env.stream(), tree, "nlp", "a"));
    std::unique_ptr<Timer> b(new Timer(env.stream(), tree, "nlp", "b"));
    advance(1ns, 0ns);
    a.reset();
    advance(2ns, 0ns);
    std::unique_ptr<Timer> c(new Timer(env.stream(), tree, "nlp", "c"));
    advance(4ns, 0ns);
    c.reset();
    b.reset();
    {
        const Timer d(env.stream(), tree, "nlp", "d");
    }
    const Texts expected{
        "a count=1 wall=1 cpu=0",
        "a/b count=1 wall=7 cpu=0",
        "a/b/c count=1 wall=4 cpu=0",
        "d count=1 wall=0 cpu=0",
    };
    EXPECT_EQ(summary(tree), expected);
}

TEST_F(TimerTest, FailedEntry)
{
    auto owned = std::make_unique<FlakyClock>();
    FlakyClock &flaky = *owned;
    Env local(std::move(owned));
    listen(local);
    flaky.broken = true;
    EXPECT_THROW(run(local, tree), Error);
    EXPECT_TRUE(tree.nodes().empty());
    EXPECT_TRUE(kept.empty());
    flaky.broken = false;
    run(local, tree);
    const Texts expected{
        "solve count=1 wall=0 cpu=0",
        "solve/iterate count=3 wall=0 cpu=0",
    };
    EXPECT_EQ(summary(tree), expected);
}

TEST_F(TimerTest, FailedLeave)
{
    static_assert(std::is_nothrow_destructible_v<Timer>);
    auto owned = std::make_unique<FlakyClock>();
    FlakyClock &flaky = *owned;
    Env local(std::move(owned));
    listen(local);
    const auto drift = [&flaky](const Event &) { flaky.cpu += 10ns; };
    local.events().add_hook(EventKind::Span, EventLevel::Debug, drift);
    {
        const Timer solve(local.stream(), tree, "nlp", "solve");
        flaky.wall += 5ns;
        flaky.cpu += 2ns;
        flaky.broken = true;
    }
    EXPECT_EQ(summary(tree), (Texts{"solve count=1 wall=5 cpu=0"}));
    const Texts expected{
        "solve edge=begin wall=0 cpu=0",
        "solve edge=end wall=5 cpu=0",
    };
    EXPECT_EQ(lines(kept), expected);
}

TEST_F(TimerTest, Entry)
{
    listen(env);
    {
        const Timer solve(env.stream(), tree, "nlp", "solve");
        advance(1ns, 1ns);
        const EntryTimer read(env.stream(), "io", "read");
        advance(2ns, 1ns);
    }
    {
        const EntryTimer read(env.stream(), "io", "read");
        advance(4ns, 2ns);
    }
    EXPECT_EQ(summary(tree), (Texts{"solve count=1 wall=3 cpu=2"}));
    const Texts expected{
        "solve edge=begin wall=0 cpu=0",
        "read edge=begin wall=1 cpu=1",
        "read edge=end wall=3 cpu=2",
        "solve edge=end wall=3 cpu=2",
        "read edge=begin wall=3 cpu=2",
        "read edge=end wall=7 cpu=4",
    };
    EXPECT_EQ(lines(kept), expected);
    EXPECT_EQ(kept[1].module, "io");
}
