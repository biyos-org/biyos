#include "core/usage.hpp"

#include <gtest/gtest.h>

#include <thread>

namespace
{

using Probe = bool (*)(biyos::Usage &);

bool enters(biyos::Usage &usage)
{
    const bool entered = usage.enter();
    if (entered)
    {
        usage.leave();
    }
    return entered;
}

bool claims(biyos::Usage &usage)
{
    if (!usage.enter())
    {
        return false;
    }
    const bool claimed = usage.claim();
    usage.leave();
    return claimed;
}

bool elsewhere(Probe probe, biyos::Usage &usage)
{
    bool result = false;
    std::thread([&] { result = probe(usage); }).join();
    return result;
}

}

TEST(UsageTest, Shared)
{
    biyos::Usage usage;
    ASSERT_TRUE(usage.enter());
    EXPECT_TRUE(elsewhere(enters, usage));
    EXPECT_FALSE(elsewhere(claims, usage));
    EXPECT_TRUE(usage.claim());
    usage.leave();
    EXPECT_TRUE(elsewhere(claims, usage));
}

TEST(UsageTest, Claim)
{
    biyos::Usage usage;
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
    biyos::Usage usage;
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
    biyos::Usage usage;
    ASSERT_TRUE(usage.enter());
    ASSERT_TRUE(usage.enter());
    EXPECT_FALSE(usage.claim());
    usage.leave();
    EXPECT_TRUE(usage.claim());
    usage.leave();
    EXPECT_TRUE(elsewhere(enters, usage));
}
