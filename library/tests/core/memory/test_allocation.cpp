#include "allocations.hpp"
#include "core/env.hpp"
#include "core/events/events.hpp"
#include "core/events/stream.hpp"
#include "core/timing/clock.hpp"
#include "core/timing/timer.hpp"
#include "delivery.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <optional>

using biyos::Env;
using biyos::ErrorKind;
using biyos::ErrorScope;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::FakeClock;
using biyos::Timer;
using biyos::TimingTree;
using biyos::test::allocations;
using biyos::test::fail_after;
using biyos::test::in_delivery;
using biyos::test::stop_failing;

namespace
{

void ignore(const Event &) {}

void run(Env &env, TimingTree &tree)
{
    const Timer solve(env.stream(), tree, "nlp", "solve");
    for (int step = 0; step < 3; ++step)
    {
        const Timer iterate(env.stream(), tree, "nlp", "iterate");
    }
}

void failed_scope(Env &env)
{
    const ErrorScope scope(env);
    fail_after(0);
    env.stream().record(ErrorKind::Internal, "inner");
    stop_failing();
}

}

TEST(AllocationTest, SecondEntry)
{
    Env env(std::make_unique<FakeClock>());
    env.events().add_hook(EventKind::Span, EventLevel::Warning, ignore);
    TimingTree tree;
    const std::size_t start = allocations();
    run(env, tree);
    const std::size_t first = allocations();
    run(env, tree);
    EXPECT_GT(first, start);
    EXPECT_EQ(allocations(), first);
}

TEST(AllocationTest, FirstError)
{
    Env env;
    fail_after(0);
    env.stream().record(ErrorKind::Internal, "first");
    stop_failing();
    EXPECT_EQ(env.last_error().kind, std::nullopt);
    EXPECT_STREQ(env.last_error().text, "");
}

TEST(AllocationTest, EmptyScope)
{
    Env env;
    in_delivery(env, failed_scope);
    EXPECT_EQ(env.last_error().kind, std::nullopt);
    EXPECT_STREQ(env.last_error().text, "");
    env.stream().record(ErrorKind::Internal, "later");
    EXPECT_EQ(env.last_error().kind, ErrorKind::Internal);
    EXPECT_STREQ(env.last_error().text, "later");
}
