#include "allocations.hpp"
#include "core/clock.hpp"
#include "core/environment.hpp"
#include "core/events.hpp"
#include "core/timer.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>

namespace
{

void ignore(const biyos::Event &) {}

void run(biyos::Environment &env, biyos::TimingTree &tree)
{
    const biyos::Timer solve(env, tree, "nlp", "solve");
    for (int step = 0; step < 3; ++step)
    {
        const biyos::Timer iterate(env, tree, "nlp", "iterate");
    }
}

}

TEST(AllocationTest, SecondEntry)
{
    biyos::Environment env(std::make_unique<biyos::FakeClock>());
    env.events().subscribe(biyos::EventKind::Phase, biyos::Level::Warning, ignore);
    biyos::TimingTree tree;
    const std::size_t start = biyos::test::allocations();
    run(env, tree);
    const std::size_t first = biyos::test::allocations();
    run(env, tree);
    EXPECT_GT(first, start);
    EXPECT_EQ(biyos::test::allocations(), first);
}
