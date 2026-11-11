#include "core/clock.hpp"
#include "core/environment.hpp"
#include "core/error.hpp"

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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
    env.events().subscribe(biyos::EventKind::Message,
                           biyos::Level::Info,
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

TEST(EnvironmentTest, Handles)
{
    biyos::Environment env;
    EXPECT_EQ(env.handles(), 0U);
    {
        const biyos::Environment::Handle first(env);
        const biyos::Environment::Handle second(env);
        EXPECT_EQ(env.handles(), 2U);
        EXPECT_EQ(&first.env(), &env);
    }
    EXPECT_EQ(env.handles(), 0U);
}

TEST(EnvironmentTest, Unmovable)
{
    EXPECT_FALSE(std::is_copy_constructible_v<biyos::Environment>);
    EXPECT_FALSE(std::is_copy_assignable_v<biyos::Environment>);
    EXPECT_FALSE(std::is_move_constructible_v<biyos::Environment>);
    EXPECT_FALSE(std::is_move_assignable_v<biyos::Environment>);
}

namespace
{

using biyos::Option;
using biyos::OptionSpec;

constexpr std::array kRows{
    OptionSpec{
        .id = Option{1},
        .name = "new",
        .rule = biyos::IntRule{.default_value = 1},
        .doc = "Current.",
        .versions = {.since = {0, 0, 1}},
    },
    OptionSpec{
        .id = Option{2},
        .name = "old",
        .rule = biyos::IntRule{.default_value = 1},
        .doc = "Deprecated.",
        .versions = {.since = {0, 0, 1}, .deprecated = biyos::Version{0, 0, 2}},
        .replacement = Option{1},
    },
};

}

TEST(EnvironmentTest, Deprecated)
{
    biyos::Environment env(std::make_unique<biyos::FakeClock>(), kRows);
    std::vector<std::string> heard;
    const auto record = [&heard](const biyos::Event &event) { heard.push_back(event.text); };
    env.events().subscribe(biyos::EventKind::Message, biyos::Level::Warning, record);
    env.options().set_int(Option{2}, 5);
    EXPECT_EQ(env.options().get_int(Option{2}), 5);
    EXPECT_EQ(heard, std::vector<std::string>{"option 'old': deprecated in 0.0.2, use 'new'"});
}
