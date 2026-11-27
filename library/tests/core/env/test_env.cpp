#include "core/env/env.hpp"
#include "core/env/error.hpp"
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

using biyos::Clock;
using biyos::Env;
using biyos::Error;
using biyos::ErrorKind;
using biyos::ErrorScope;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::FakeClock;
using biyos::IntRule;
using biyos::kRecords;
using biyos::Option;
using biyos::Scope;
using biyos::Version;
using biyos::test::in_delivery;

namespace
{

void two_errors(Env &env)
{
    const ErrorScope scope(env);
    env.stream().record(ErrorKind::InvalidArgument, "inner");
    env.stream().record(ErrorKind::InvalidArgument, "later");
    EXPECT_STREQ(env.last_error().text, "later");
}

void inner_scope(Env &env)
{
    const ErrorScope inner(env);
    env.stream().record(ErrorKind::InvalidArgument, "inner");
    EXPECT_STREQ(env.last_error().text, "inner");
}

void middle_scope(Env &env)
{
    const ErrorScope scope(env);
    env.stream().record(ErrorKind::InvalidArgument, "middle");
    inner_scope(env);
    EXPECT_STREQ(env.last_error().text, "middle");
}

void clean_scope(Env &env)
{
    const ErrorScope scope(env);
    EXPECT_STREQ(env.last_error().text, "outer");
}

void internal_scope(Env &env)
{
    const ErrorScope scope(env);
    env.stream().record(ErrorKind::Internal, "inner");
    EXPECT_EQ(env.last_error().kind, ErrorKind::Internal);
}

void fail_worker(Env &env)
{
    env.stream().record(ErrorKind::Internal, "worker");
}

void fail_together(Env &env, std::latch &all)
{
    env.stream().record(ErrorKind::Internal, "crowd");
    all.arrive_and_wait();
}

void crowded_scope(Env &env)
{
    const ErrorScope scope(env);
    env.stream().record(ErrorKind::InvalidArgument, "inner");
    std::latch all(kRecords);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < kRecords; ++i)
    {
        threads.emplace_back(fail_together, std::ref(env), std::ref(all));
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(env.last_error().kind, std::nullopt);
}

void hold(Env &env, std::latch &held, std::latch &release)
{
    const Env::Handle handle(env);
    held.count_down();
    release.wait();
}

void shared_scope(Env &env)
{
    const ErrorScope scope(env);
    std::thread(fail_worker, std::ref(env)).join();
    env.stream().record(ErrorKind::InvalidArgument, "inner");
    EXPECT_STREQ(env.last_error().text, "inner");
}

}

TEST(EnvTest, NoError)
{
    const Env env;
    EXPECT_STREQ(env.last_error().text, "");
}

TEST(EnvTest, LastError)
{
    Env env;
    env.stream().record(ErrorKind::InvalidArgument, "first");
    env.stream().record(ErrorKind::InvalidArgument, "second");
    EXPECT_STREQ(env.last_error().text, "second");
}

TEST(EnvTest, OwnsOptions)
{
    Env env;
    env.options().set_int(Option::Seed, 9);
    const Env &view = env;
    EXPECT_EQ(view.options().get_int(Option::Seed), 9);
}

TEST(EnvTest, SystemClock)
{
    const Env env;
    EXPECT_NE(dynamic_cast<const biyos::SystemClock *>(&env.clock()), nullptr);
}

TEST(EnvTest, OwnsClock)
{
    auto fake = std::make_unique<FakeClock>();
    const Clock *installed = fake.get();
    const Env env(std::move(fake));
    EXPECT_EQ(&env.clock(), installed);
}

TEST(EnvTest, NullClock)
{
    try
    {
        const Env env(nullptr);
        ADD_FAILURE() << "no error";
    }
    catch (const Error &error)
    {
        EXPECT_EQ(error.kind(), ErrorKind::InvalidArgument);
        EXPECT_STREQ(error.what(), "invalid argument: clock: is null");
    }
}

TEST(EnvTest, OwnsEvents)
{
    Env env;
    env.events().add_hook(EventKind::Message, EventLevel::Info, [](const Event &) {});
    const Env &view = env;
    EXPECT_TRUE(view.events().wanted(EventKind::Message, EventLevel::Info));
}

TEST(EnvTest, InterruptCount)
{
    Env env;
    const Env &view = env;
    EXPECT_EQ(view.interrupt().count(), 0U);
    env.interrupt().request();
    EXPECT_EQ(view.interrupt().count(), 1U);
    env.interrupt().request();
    EXPECT_EQ(view.interrupt().count(), 2U);
}

TEST(EnvTest, ScopedError)
{
    Env env;
    env.stream().record(ErrorKind::InvalidArgument, "outer");
    in_delivery(env, two_errors);
    EXPECT_STREQ(env.last_error().text, "outer");
}

TEST(EnvTest, NestedScope)
{
    Env env;
    env.stream().record(ErrorKind::InvalidArgument, "outer");
    in_delivery(env, middle_scope);
    EXPECT_STREQ(env.last_error().text, "outer");
}

TEST(EnvTest, CleanScope)
{
    Env env;
    env.stream().record(ErrorKind::InvalidArgument, "outer");
    in_delivery(env, clean_scope);
    EXPECT_STREQ(env.last_error().text, "outer");
}

TEST(EnvTest, IdleScope)
{
    Env env;
    env.stream().record(ErrorKind::InvalidArgument, "outer");
    two_errors(env);
    EXPECT_STREQ(env.last_error().text, "later");
}

TEST(EnvTest, CrowdedScope)
{
    Env env;
    env.stream().record(ErrorKind::InvalidArgument, "outer");
    in_delivery(env, crowded_scope);
    EXPECT_EQ(env.last_error().kind, ErrorKind::InvalidArgument);
    EXPECT_STREQ(env.last_error().text, "outer");
}

TEST(EnvTest, ThreadScope)
{
    Env env;
    env.stream().record(ErrorKind::InvalidArgument, "outer");
    in_delivery(env, shared_scope);
    EXPECT_STREQ(env.last_error().text, "outer");
}

TEST(EnvTest, Handles)
{
    Env env;
    EXPECT_EQ(env.handles(), 0U);
    {
        const Env::Handle first(env);
        const Env::Handle second(env);
        EXPECT_EQ(env.handles(), 2U);
        EXPECT_EQ(&first.env(), &env);
    }
    EXPECT_EQ(env.handles(), 0U);
}

TEST(EnvTest, HandleThreads)
{
    constexpr std::size_t kThreads = 8;
    Env env;
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
    EXPECT_FALSE(std::is_copy_constructible_v<Env>);
    EXPECT_FALSE(std::is_copy_assignable_v<Env>);
    EXPECT_FALSE(std::is_move_constructible_v<Env>);
    EXPECT_FALSE(std::is_move_assignable_v<Env>);
}

namespace
{

using biyos::OptionSpec;

constexpr std::array kRows{
    OptionSpec{
        .id = Option{1},
        .name = "new",
        .scope = Scope::Model,
        .rule = IntRule{.default_value = 1},
        .doc = "Current.",
        .versions = {.since = {0, 0, 1}},
    },
    OptionSpec{
        .id = Option{2},
        .name = "old",
        .scope = Scope::Model,
        .rule = IntRule{.default_value = 1},
        .doc = "Deprecated.",
        .versions = {.since = {0, 0, 1}, .deprecated = Version{0, 0, 2}},
        .replacement = Option{1},
    },
};

}

TEST(EnvTest, Deprecated)
{
    Env env(std::make_unique<FakeClock>(), kRows);
    std::vector<std::string> heard;
    const auto record = [&heard](const Event &event) { heard.push_back(event.text); };
    env.events().add_hook(EventKind::Message, EventLevel::Warning, record);
    env.options().set_int(Option{2}, 5);
    EXPECT_EQ(env.options().get_int(Option{2}), 5);
    EXPECT_EQ(heard, std::vector<std::string>{"option 'old': deprecated in 0.0.2, use 'new'"});
}

TEST(EnvTest, ErrorStatus)
{
    Env env;
    EXPECT_EQ(env.last_error().kind, std::nullopt);
    env.stream().record(ErrorKind::NoMemory, "full");
    EXPECT_EQ(env.last_error().kind, ErrorKind::NoMemory);
    EXPECT_EQ(env.last_error().model, nullptr);
}

TEST(EnvTest, ThreadError)
{
    Env env;
    env.stream().record(ErrorKind::InvalidArgument, "main");
    std::string before;
    std::string after;
    std::thread worker(
        [&]
        {
            before = env.last_error().text;
            env.stream().record(ErrorKind::Internal, "worker");
            after = env.last_error().text;
        });
    worker.join();
    EXPECT_EQ(before, "");
    EXPECT_EQ(after, "worker");
    EXPECT_STREQ(env.last_error().text, "main");
    EXPECT_EQ(env.last_error().kind, ErrorKind::InvalidArgument);
}

TEST(EnvTest, ScopedStatus)
{
    Env env;
    env.stream().record(ErrorKind::NoMemory, "outer");
    in_delivery(env, internal_scope);
    EXPECT_EQ(env.last_error().kind, ErrorKind::NoMemory);
}
