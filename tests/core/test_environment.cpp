#include "core/clock.hpp"
#include "core/environment.hpp"
#include "core/error.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <utility>

TEST(EnvironmentTest, NoError)
{
    const biyos::Environment env;
    EXPECT_EQ(env.last_error(), "");
}

TEST(EnvironmentTest, LastError)
{
    biyos::Environment env;
    env.record_error("first");
    env.record_error("second");
    EXPECT_EQ(env.last_error(), "second");
}

TEST(EnvironmentTest, OwnsOptions)
{
    biyos::Environment env;
    env.options().set_int(biyos::Option::Seed, 9);
    const biyos::Environment &view = env;
    EXPECT_EQ(view.options().get_int(biyos::Option::Seed), 9);
}

TEST(EnvironmentTest, SystemClock)
{
    const biyos::Environment env;
    EXPECT_NE(dynamic_cast<const biyos::SystemClock *>(&env.clock()), nullptr);
}

TEST(EnvironmentTest, OwnsClock)
{
    auto fake = std::make_unique<biyos::FakeClock>();
    const biyos::Clock *installed = fake.get();
    const biyos::Environment env(std::move(fake));
    EXPECT_EQ(&env.clock(), installed);
}

TEST(EnvironmentTest, NullClock)
{
    try
    {
        const biyos::Environment env(nullptr);
        ADD_FAILURE() << "no error";
    }
    catch (const biyos::Error &error)
    {
        EXPECT_EQ(error.kind(), biyos::ErrorKind::InvalidArgument);
        EXPECT_STREQ(error.what(), "invalid argument: clock: is null");
    }
}

TEST(EnvironmentTest, OwnsEvents)
{
    biyos::Environment env;
    env.events().subscribe(biyos::EventKind::Message, biyos::Level::Info,
                           [](const biyos::Event &) {});
    const biyos::Environment &view = env;
    EXPECT_TRUE(view.events().wanted(biyos::EventKind::Message, biyos::Level::Info));
}

TEST(EnvironmentTest, ScopedError)
{
    biyos::Environment env;
    env.record_error("outer");
    {
        const biyos::Environment::ErrorScope scope(env);
        env.record_error("inner");
        env.record_error("later");
        EXPECT_EQ(env.last_error(), "later");
    }
    EXPECT_EQ(env.last_error(), "outer");
}

TEST(EnvironmentTest, NestedScope)
{
    biyos::Environment env;
    env.record_error("outer");
    {
        const biyos::Environment::ErrorScope scope(env);
        env.record_error("middle");
        {
            const biyos::Environment::ErrorScope inner(env);
            env.record_error("inner");
            EXPECT_EQ(env.last_error(), "inner");
        }
        EXPECT_EQ(env.last_error(), "middle");
    }
    EXPECT_EQ(env.last_error(), "outer");
}

TEST(EnvironmentTest, CleanScope)
{
    biyos::Environment env;
    env.record_error("outer");
    {
        const biyos::Environment::ErrorScope scope(env);
        EXPECT_EQ(env.last_error(), "outer");
    }
    EXPECT_EQ(env.last_error(), "outer");
}
