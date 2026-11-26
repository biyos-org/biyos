#include "core/memory/usage.hpp"

#include <gtest/gtest.h>

#include <thread>

using biyos::Usage;

namespace
{

using Probe = bool (*)(Usage &);

bool enters(Usage &usage)
{
    const bool entered = usage.enter();
    if (entered)
    {
        usage.leave();
    }
    return entered;
}

bool claims(Usage &usage)
{
    if (!usage.enter())
    {
        return false;
    }
    const bool claimed = usage.claim();
    usage.leave();
    return claimed;
}

bool holds(Usage &usage)
{
    return usage.held();
}

bool elsewhere(Probe probe, Usage &usage)
{
    bool result = false;
    std::thread([&] { result = probe(usage); }).join();
    return result;
}

}

TEST(UsageTest, Shared)
{
    Usage usage;
    ASSERT_TRUE(usage.enter());
    EXPECT_TRUE(elsewhere(enters, usage));
    EXPECT_FALSE(elsewhere(claims, usage));
    EXPECT_TRUE(usage.claim());
    usage.leave();
    EXPECT_TRUE(elsewhere(claims, usage));
}

TEST(UsageTest, Claim)
{
    Usage usage;
    ASSERT_TRUE(usage.enter());
    ASSERT_TRUE(usage.claim());
    EXPECT_TRUE(usage.claim());
    EXPECT_FALSE(elsewhere(enters, usage));
    EXPECT_FALSE(elsewhere(claims, usage));
    usage.leave();
    EXPECT_TRUE(elsewhere(enters, usage));
    EXPECT_TRUE(elsewhere(claims, usage));
}

TEST(UsageTest, Nested)
{
    Usage usage;
    ASSERT_TRUE(usage.enter());
    ASSERT_TRUE(usage.claim());
    ASSERT_TRUE(usage.enter());
    EXPECT_TRUE(usage.claim());
    usage.leave();
    EXPECT_FALSE(elsewhere(enters, usage));
    usage.leave();
    EXPECT_TRUE(elsewhere(enters, usage));
}

TEST(UsageTest, Outer)
{
    Usage usage;
    ASSERT_TRUE(usage.enter());
    ASSERT_TRUE(usage.enter());
    EXPECT_FALSE(usage.claim());
    usage.leave();
    EXPECT_TRUE(usage.claim());
    usage.leave();
    EXPECT_TRUE(elsewhere(enters, usage));
}

TEST(UsageTest, Held)
{
    Usage usage;
    ASSERT_TRUE(usage.enter());
    EXPECT_FALSE(usage.held());
    ASSERT_TRUE(usage.claim());
    EXPECT_TRUE(usage.held());
    EXPECT_FALSE(elsewhere(holds, usage));
    usage.leave();
    EXPECT_FALSE(usage.held());
}
