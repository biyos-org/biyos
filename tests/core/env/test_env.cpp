#include "core/env.hpp"
#include "core/error.hpp"
#include "core/events/records.hpp"
#include "core/events/stream.hpp"
#include "core/timing/clock.hpp"
#include "delivery.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <functional>
#include <latch>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

void two_errors(biyos::Env &env)
{
    const biyos::ErrorScope scope(env);
    env.stream().record(biyos::ErrorKind::InvalidArgument, "inner");
    env.stream().record(biyos::ErrorKind::InvalidArgument, "later");
    EXPECT_STREQ(env.last_error().text, "later");
}

void inner_scope(biyos::Env &env)
{
    const biyos::ErrorScope inner(env);
    env.stream().record(biyos::ErrorKind::InvalidArgument, "inner");
    EXPECT_STREQ(env.last_error().text, "inner");
}

void middle_scope(biyos::Env &env)
{
    const biyos::ErrorScope scope(env);
    env.stream().record(biyos::ErrorKind::InvalidArgument, "middle");
    inner_scope(env);
    EXPECT_STREQ(env.last_error().text, "middle");
}

void clean_scope(biyos::Env &env)
{
    const biyos::ErrorScope scope(env);
    EXPECT_STREQ(env.last_error().text, "outer");
}

void internal_scope(biyos::Env &env)
{
    const biyos::ErrorScope scope(env);
    env.stream().record(biyos::ErrorKind::Internal, "inner");
    EXPECT_EQ(env.last_error().kind, biyos::ErrorKind::Internal);
}

void fail_worker(biyos::Env &env)
{
    env.stream().record(biyos::ErrorKind::Internal, "worker");
}

void fail_together(biyos::Env &env, std::latch &all)
{
    env.stream().record(biyos::ErrorKind::Internal, "crowd");
    all.arrive_and_wait();
}

void crowded_scope(biyos::Env &env)
{
    const biyos::ErrorScope scope(env);
    env.stream().record(biyos::ErrorKind::InvalidArgument, "inner");
    std::latch all(biyos::kRecords);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < biyos::kRecords; ++i)
    {
        threads.emplace_back(fail_together, std::ref(env), std::ref(all));
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(env.last_error().kind, std::nullopt);
}

void hold(biyos::Env &env, std::latch &held, std::latch &release)
{
    const biyos::Env::Handle handle(env);
    held.count_down();
    release.wait();
}

void shared_scope(biyos::Env &env)
{
    const biyos::ErrorScope scope(env);
    std::thread(fail_worker, std::ref(env)).join();
    env.stream().record(biyos::ErrorKind::InvalidArgument, "inner");
    EXPECT_STREQ(env.last_error().text, "inner");
}

}

TEST(EnvTest, NoError)
{
    const biyos::Env env;
    EXPECT_STREQ(env.last_error().text, "");
}

TEST(EnvTest, LastError)
{
    biyos::Env env;
    env.stream().record(biyos::ErrorKind::InvalidArgument, "first");
    env.stream().record(biyos::ErrorKind::InvalidArgument, "second");
    EXPECT_STREQ(env.last_error().text, "second");
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
    env.stream().record(biyos::ErrorKind::InvalidArgument, "outer");
    biyos::test::in_delivery(env, two_errors);
    EXPECT_STREQ(env.last_error().text, "outer");
}

TEST(EnvTest, NestedScope)
{
    biyos::Env env;
    env.stream().record(biyos::ErrorKind::InvalidArgument, "outer");
    biyos::test::in_delivery(env, middle_scope);
    EXPECT_STREQ(env.last_error().text, "outer");
}

TEST(EnvTest, CleanScope)
{
    biyos::Env env;
    env.stream().record(biyos::ErrorKind::InvalidArgument, "outer");
    biyos::test::in_delivery(env, clean_scope);
    EXPECT_STREQ(env.last_error().text, "outer");
}

TEST(EnvTest, IdleScope)
{
    biyos::Env env;
    env.stream().record(biyos::ErrorKind::InvalidArgument, "outer");
    two_errors(env);
    EXPECT_STREQ(env.last_error().text, "later");
}

TEST(EnvTest, CrowdedScope)
{
    biyos::Env env;
    env.stream().record(biyos::ErrorKind::InvalidArgument, "outer");
    biyos::test::in_delivery(env, crowded_scope);
    EXPECT_EQ(env.last_error().kind, biyos::ErrorKind::InvalidArgument);
    EXPECT_STREQ(env.last_error().text, "outer");
}

TEST(EnvTest, ThreadScope)
{
    biyos::Env env;
    env.stream().record(biyos::ErrorKind::InvalidArgument, "outer");
    biyos::test::in_delivery(env, shared_scope);
    EXPECT_STREQ(env.last_error().text, "outer");
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

TEST(EnvTest, HandleThreads)
{
    constexpr std::size_t kThreads = 8;
    biyos::Env env;
    std::latch held(kThreads);
    std::latch release(1);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < kThreads; ++i)
    {
        threads.emplace_back(hold, std::ref(env), std::ref(held), std::ref(release));
    }
    held.wait();
    EXPECT_EQ(env.handles(), kThreads);
    release.count_down();
    for (std::thread &thread : threads)
    {
        thread.join();
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
    EXPECT_EQ(env.last_error().kind, std::nullopt);
    env.stream().record(biyos::ErrorKind::NoMemory, "full");
    EXPECT_EQ(env.last_error().kind, biyos::ErrorKind::NoMemory);
    EXPECT_EQ(env.last_error().model, nullptr);
}

TEST(EnvTest, ThreadError)
{
    biyos::Env env;
    env.stream().record(biyos::ErrorKind::InvalidArgument, "main");
    std::string before;
    std::string after;
    std::thread worker(
        [&]
        {
            before = env.last_error().text;
            env.stream().record(biyos::ErrorKind::Internal, "worker");
            after = env.last_error().text;
        });
    worker.join();
    EXPECT_EQ(before, "");
    EXPECT_EQ(after, "worker");
    EXPECT_STREQ(env.last_error().text, "main");
    EXPECT_EQ(env.last_error().kind, biyos::ErrorKind::InvalidArgument);
}

TEST(EnvTest, ScopedStatus)
{
    biyos::Env env;
    env.stream().record(biyos::ErrorKind::NoMemory, "outer");
    biyos::test::in_delivery(env, internal_scope);
    EXPECT_EQ(env.last_error().kind, biyos::ErrorKind::NoMemory);
}
