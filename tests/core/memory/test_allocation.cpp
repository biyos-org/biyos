#include "allocations.hpp"
#include "core/clock.hpp"
#include "core/env.hpp"
#include "core/events/events.hpp"
#include "core/timer.hpp"
#include "delivery.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>

namespace
{

void ignore(const biyos::Event &) {}

void run(biyos::Env &env, biyos::TimingTree &tree)
{
    const biyos::Timer solve(env, tree, "nlp", "solve");
    for (int step = 0; step < 3; ++step)
    {
        const biyos::Timer iterate(env, tree, "nlp", "iterate");
    }
}

void failed_scope(biyos::Env &env)
{
    const biyos::ErrorScope scope(env);
    biyos::test::fail_after(0);
    env.record_error(biyos::ErrorKind::Internal, "inner");
    biyos::test::stop_failing();
}

}

TEST(AllocationTest, SecondEntry)
{
    biyos::Env env(std::make_unique<biyos::FakeClock>());
    env.events().add_hook(biyos::EventKind::Span, biyos::EventLevel::Warning, ignore);
    biyos::TimingTree tree;
    const std::size_t start = biyos::test::allocations();
    run(env, tree);
    const std::size_t first = biyos::test::allocations();
    run(env, tree);
    EXPECT_GT(first, start);
    EXPECT_EQ(biyos::test::allocations(), first);
}

TEST(AllocationTest, FirstError)
{
    biyos::Env env;
    biyos::test::fail_after(0);
    env.record_error(biyos::ErrorKind::Internal, "first");
    biyos::test::stop_failing();
    EXPECT_EQ(env.last_error().status, BIYOS_OK);
    EXPECT_STREQ(env.last_error().text, "");
}

TEST(AllocationTest, EmptyScope)
{
    biyos::Env env;
    biyos::test::in_delivery(env, failed_scope);
    EXPECT_EQ(env.last_error().status, BIYOS_OK);
    EXPECT_STREQ(env.last_error().text, "");
    env.record_error(biyos::ErrorKind::Internal, "later");
    EXPECT_EQ(env.last_error().status, BIYOS_ERR_INTERNAL);
    EXPECT_STREQ(env.last_error().text, "later");
}

TEST(AllocationTest, ScopedError)
{
    const int address = 0;
    const auto *model = reinterpret_cast<const BIYOS_model *>(&address);
    const std::string outer(64, 'o');
    biyos::Env env;
    env.record_error(biyos::ErrorKind::NoMemory, outer, model);
    biyos::test::in_delivery(env, failed_scope);
    EXPECT_EQ(env.last_error().status, BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.last_error().model, model);
    EXPECT_STREQ(env.last_error().text, outer.c_str());
}
