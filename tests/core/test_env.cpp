#include "core/clock.hpp"
#include "core/env.hpp"
#include "core/error.hpp"

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

TEST(EnvTest, NoError)
{
    const biyos::Env env;
    EXPECT_EQ(env.last_error().text, "");
}

TEST(EnvTest, LastError)
{
    biyos::Env env;
    env.record_error(biyos::ErrorKind::InvalidArgument, "first");
    env.record_error(biyos::ErrorKind::InvalidArgument, "second");
    EXPECT_EQ(env.last_error().text, "second");
}

TEST(EnvTest, OwnsOptions)
{
    biyos::Env env;
    env.options().set_int(biyos::Option::Seed, 9);
    const biyos::Env &view = env;
    EXPECT_EQ(view.options().get_int(biyos::Option::Seed), 9);
}

TEST(EnvTest, SystemClock)
{
    const biyos::Env env;
    EXPECT_NE(dynamic_cast<const biyos::SystemClock *>(&env.clock()), nullptr);
}

TEST(EnvTest, OwnsClock)
{
    auto fake = std::make_unique<biyos::FakeClock>();
    const biyos::Clock *installed = fake.get();
    const biyos::Env env(std::move(fake));
    EXPECT_EQ(&env.clock(), installed);
}

TEST(EnvTest, NullClock)
{
    try
    {
        const biyos::Env env(nullptr);
        ADD_FAILURE() << "no error";
    }
    catch (const biyos::Error &error)
    {
        EXPECT_EQ(error.kind(), biyos::ErrorKind::InvalidArgument);
        EXPECT_STREQ(error.what(), "invalid argument: clock: is null");
    }
}

TEST(EnvTest, OwnsEvents)
{
    biyos::Env env;
    env.events().add_hook(biyos::EventKind::Message,
                          biyos::EventLevel::Info,
                          [](const biyos::Event &) {});
    const biyos::Env &view = env;
    EXPECT_TRUE(view.events().wanted(biyos::EventKind::Message, biyos::EventLevel::Info));
}

TEST(EnvTest, InterruptFlag)
{
    biyos::Env env;
    const biyos::Env &view = env;
    EXPECT_FALSE(view.interrupt().is_set());
    env.interrupt().set();
    EXPECT_TRUE(view.interrupt().is_set());
    env.interrupt().set();
    EXPECT_TRUE(view.interrupt().is_set());
    env.interrupt().clear();
    EXPECT_FALSE(view.interrupt().is_set());
}

TEST(EnvTest, ScopedError)
{
    biyos::Env env;
    env.record_error(biyos::ErrorKind::InvalidArgument, "outer");
    {
        const biyos::Env::ErrorScope scope(env);
        env.record_error(biyos::ErrorKind::InvalidArgument, "inner");
        env.record_error(biyos::ErrorKind::InvalidArgument, "later");
        EXPECT_EQ(env.last_error().text, "later");
    }
    EXPECT_EQ(env.last_error().text, "outer");
}

TEST(EnvTest, NestedScope)
{
    biyos::Env env;
    env.record_error(biyos::ErrorKind::InvalidArgument, "outer");
    {
        const biyos::Env::ErrorScope scope(env);
        env.record_error(biyos::ErrorKind::InvalidArgument, "middle");
        {
            const biyos::Env::ErrorScope inner(env);
            env.record_error(biyos::ErrorKind::InvalidArgument, "inner");
            EXPECT_EQ(env.last_error().text, "inner");
        }
        EXPECT_EQ(env.last_error().text, "middle");
    }
    EXPECT_EQ(env.last_error().text, "outer");
}

TEST(EnvTest, CleanScope)
{
    biyos::Env env;
    env.record_error(biyos::ErrorKind::InvalidArgument, "outer");
    {
        const biyos::Env::ErrorScope scope(env);
        EXPECT_EQ(env.last_error().text, "outer");
    }
    EXPECT_EQ(env.last_error().text, "outer");
}

TEST(EnvTest, Handles)
{
    biyos::Env env;
    EXPECT_EQ(env.handles(), 0U);
    {
        const biyos::Env::Handle first(env);
        const biyos::Env::Handle second(env);
        EXPECT_EQ(env.handles(), 2U);
        EXPECT_EQ(&first.env(), &env);
    }
    EXPECT_EQ(env.handles(), 0U);
}

TEST(EnvTest, Unmovable)
{
    EXPECT_FALSE(std::is_copy_constructible_v<biyos::Env>);
    EXPECT_FALSE(std::is_copy_assignable_v<biyos::Env>);
    EXPECT_FALSE(std::is_move_constructible_v<biyos::Env>);
    EXPECT_FALSE(std::is_move_assignable_v<biyos::Env>);
}

namespace
{

using biyos::Option;
using biyos::OptionSpec;

constexpr std::array kRows{
    OptionSpec{
        .id = Option{1},
        .name = "new",
        .scope = biyos::Scope::Model,
        .rule = biyos::IntRule{.default_value = 1},
        .doc = "Current.",
        .versions = {.since = {0, 0, 1}},
    },
    OptionSpec{
        .id = Option{2},
        .name = "old",
        .scope = biyos::Scope::Model,
        .rule = biyos::IntRule{.default_value = 1},
        .doc = "Deprecated.",
        .versions = {.since = {0, 0, 1}, .deprecated = biyos::Version{0, 0, 2}},
        .replacement = Option{1},
    },
};

}

TEST(EnvTest, Deprecated)
{
    biyos::Env env(std::make_unique<biyos::FakeClock>(), kRows);
    std::vector<std::string> heard;
    const auto record = [&heard](const biyos::Event &event) { heard.push_back(event.text); };
    env.events().add_hook(biyos::EventKind::Message, biyos::EventLevel::Warning, record);
    env.options().set_int(Option{2}, 5);
    EXPECT_EQ(env.options().get_int(Option{2}), 5);
    EXPECT_EQ(heard, std::vector<std::string>{"option 'old': deprecated in 0.0.2, use 'new'"});
}

TEST(EnvTest, ErrorStatus)
{
    biyos::Env env;
    EXPECT_EQ(env.last_error().status, BIYOS_OK);
    env.record_error(biyos::ErrorKind::NoMemory, "full");
    EXPECT_EQ(env.last_error().status, BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.last_error().model, nullptr);
}

TEST(EnvTest, ThreadError)
{
    biyos::Env env;
    env.record_error(biyos::ErrorKind::InvalidArgument, "main");
    std::string before;
    std::string after;
    std::thread worker(
        [&]
        {
            before = env.last_error().text;
            env.record_error(biyos::ErrorKind::Internal, "worker");
            after = env.last_error().text;
        });
    worker.join();
    EXPECT_EQ(before, "");
    EXPECT_EQ(after, "worker");
    EXPECT_EQ(env.last_error().text, "main");
    EXPECT_EQ(env.last_error().status, BIYOS_ERR_BADARG);
}

TEST(EnvTest, ScopedStatus)
{
    biyos::Env env;
    env.record_error(biyos::ErrorKind::NoMemory, "outer");
    {
        const biyos::Env::ErrorScope scope(env);
        env.record_error(biyos::ErrorKind::Internal, "inner");
        EXPECT_EQ(env.last_error().status, BIYOS_ERR_INTERNAL);
    }
    EXPECT_EQ(env.last_error().status, BIYOS_ERR_NOMEMORY);
}

TEST(EnvTest, ErrorModel)
{
    const int address = 0;
    const auto *model = reinterpret_cast<const BIYOS_model *>(&address);
    biyos::Env env;
    {
        const biyos::Events::Caller caller(env.events(), model);
        env.record_error(biyos::ErrorKind::InvalidArgument, "model");
        EXPECT_EQ(env.last_error().model, model);
        {
            const biyos::Env::ErrorScope scope(env);
            const biyos::Events::Caller none(env.events(), nullptr);
            env.record_error(biyos::ErrorKind::InvalidArgument, "env");
            EXPECT_EQ(env.last_error().model, nullptr);
        }
    }
    EXPECT_EQ(env.last_error().model, model);
    EXPECT_EQ(env.last_error().text, "model");
}
