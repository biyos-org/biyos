#include "core/clock.hpp"
#include "core/environment.hpp"
#include "core/error.hpp"
#include "core/events.hpp"
#include "core/timer.hpp"

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
using biyos::EventKind;
using biyos::Level;
using biyos::Timer;
using biyos::TimingTree;
using Texts = std::vector<std::string>;

class FlakyClock final : public biyos::Clock
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
        throw biyos::Error(biyos::ErrorKind::Internal, "clock", "cannot read the CPU time");
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

std::string line_of(const biyos::Event &event)
{
    std::string line = event.text;
    for (const biyos::Field &field : event.fields)
    {
        line += " " + field.name + "=" + field.value;
    }
    return line;
}

Texts lines(const std::vector<biyos::Event> &events)
{
    Texts lines;
    for (const biyos::Event &event : events)
    {
        lines.push_back(line_of(event));
    }
    return lines;
}

void run(biyos::Environment &env, TimingTree &tree)
{
    const Timer solve(env, tree, "nlp", "solve");
    for (int step = 0; step < 3; ++step)
    {
        const Timer iterate(env, tree, "nlp", "iterate");
    }
}

void abandon(biyos::Environment &env, TimingTree &tree, biyos::FakeClock &clock)
{
    const Timer solve(env, tree, "nlp", "solve");
    clock.advance_wall(3ns);
    throw std::runtime_error("stop");
}

class TimerTest : public ::testing::Test
{
  protected:
    TimerTest() : TimerTest(std::make_unique<biyos::FakeClock>()) {}

    void advance(std::chrono::nanoseconds wall, std::chrono::nanoseconds cpu)
    {
        clock.advance_wall(wall);
        clock.advance_cpu(cpu);
    }

    void lag(const biyos::Event &event)
    {
        kept.push_back(event);
        advance(10ns, 10ns);
    }

    void listen(biyos::Environment &target)
    {
        const auto keep = [this](const biyos::Event &event) { kept.push_back(event); };
        target.events().subscribe(EventKind::Phase, Level::Debug, keep);
    }

    biyos::FakeClock &clock;
    biyos::Environment env;
    TimingTree tree;
    std::vector<biyos::Event> kept;

  private:
    explicit TimerTest(std::unique_ptr<biyos::FakeClock> owned)
        : clock(*owned), env(std::move(owned))
    {
    }
};

}

TEST_F(TimerTest, NestedScopes)
{
    {
        const Timer solve(env, tree, "nlp", "solve");
        advance(1ns, 1ns);
        {
            const Timer presolve(env, tree, "nlp", "presolve");
            advance(2ns, 1ns);
            const Timer factor(env, tree, "nlp", "factor");
            advance(4ns, 2ns);
        }
        {
            const Timer root(env, tree, "nlp", "root");
            const Timer factor(env, tree, "nlp", "factor");
            advance(8ns, 4ns);
        }
    }
    {
        const Timer postsolve(env, tree, "nlp", "postsolve");
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
        const Timer solve(env, tree, "nlp", "solve");
        advance(1ns, 1ns);
        for (int step = 0; step < 3; ++step)
        {
            const Timer iterate(env, tree, "nlp", "iterate");
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
    auto owned = std::make_unique<biyos::FakeClock>();
    biyos::FakeClock &fake = *owned;
    fake.advance_wall(7s);
    fake.advance_cpu(3ns);
    biyos::Environment local(std::move(owned));
    listen(local);
    {
        fake.advance_wall(5ns);
        const Timer solve(local, tree, "nlp", "solve");
        fake.advance_wall(3ns);
        fake.advance_cpu(2ns);
        const Timer presolve(local, tree, "nlp", "presolve");
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
    for (const biyos::Event &event : kept)
    {
        EXPECT_EQ(event.kind, EventKind::Phase);
        EXPECT_EQ(event.level, Level::Info);
        EXPECT_EQ(event.module, "nlp");
    }
    EXPECT_EQ(kept.back().time, 12ns);
}

TEST_F(TimerTest, SlowSubscriber)
{
    const auto slow = [this](const biyos::Event &event) { lag(event); };
    env.events().subscribe(EventKind::Phase, Level::Debug, slow);
    {
        const Timer solve(env, tree, "nlp", "solve");
        advance(1ns, 1ns);
        const Timer presolve(env, tree, "nlp", "presolve");
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
        const Timer postsolve(env, tree, "nlp", "postsolve");
    }
    const Texts expected{
        "solve count=1 wall=3 cpu=0",
        "postsolve count=1 wall=0 cpu=0",
    };
    EXPECT_EQ(summary(tree), expected);
}

TEST_F(TimerTest, FailedEntry)
{
    auto owned = std::make_unique<FlakyClock>();
    FlakyClock &flaky = *owned;
    biyos::Environment local(std::move(owned));
    listen(local);
    flaky.broken = true;
    EXPECT_THROW(run(local, tree), biyos::Error);
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
    biyos::Environment local(std::move(owned));
    listen(local);
    const auto drift = [&flaky](const biyos::Event &) { flaky.cpu += 10ns; };
    local.events().subscribe(EventKind::Phase, Level::Debug, drift);
    {
        const Timer solve(local, tree, "nlp", "solve");
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
