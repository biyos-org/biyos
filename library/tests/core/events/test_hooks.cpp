#include "core/env/error.hpp"
#include "core/events/event.hpp"
#include "core/events/hooks.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <latch>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{

using biyos::Error;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::Hook;
using biyos::Hooks;
using Texts = std::vector<std::string>;

constexpr int kThreads = 4;
constexpr int kRounds = 200;

std::string failure(const std::function<void()> &body)
{
    try
    {
        body();
    }
    catch (const Error &error)
    {
        return error.what();
    }
    return "no error";
}

void ignore(const Event &) {}

void fail(const Event &)
{
    throw std::runtime_error("hook");
}

struct Holder
{
    std::shared_ptr<int> held;

    void operator()(const Event &) const {}
};

class Watch
{
  public:
    void operator()(const Event &)
    {
        if (inside_.exchange(true))
        {
            ++overlaps_;
        }
        std::this_thread::yield();
        ++seen_;
        inside_.store(false);
    }

    int overlaps() const { return overlaps_.load(); }
    int seen() const { return seen_; }

  private:
    std::atomic<bool> inside_{false};
    std::atomic<int> overlaps_{0};
    int seen_ = 0;
};

Event message(EventLevel level, const std::string &text)
{
    return Event{
        .kind = EventKind::Message,
        .level = level,
        .time = {},
        .module = {},
        .text = text,
        .fields = {},
        .model = nullptr,
    };
}

void chatter(Hooks &hooks, std::latch &start)
{
    const Event event = message(EventLevel::Info, "chatter");
    start.arrive_and_wait();
    for (int i = 0; i < kRounds; ++i)
    {
        hooks.deliver(event);
    }
}

void churn(Hooks &hooks, std::latch &start)
{
    start.arrive_and_wait();
    for (int i = 0; i < kRounds; ++i)
    {
        hooks.delete_hook(hooks.add_hook(EventKind::Message, EventLevel::Debug, ignore));
    }
}

bool elsewhere(const Hooks &hooks)
{
    bool seen = true;
    std::thread([&] { seen = hooks.delivering(); }).join();
    return seen;
}

class HookTest : public ::testing::Test
{
  protected:
    int listen(EventLevel level, Hook hook)
    {
        return hooks.add_hook(EventKind::Message, level, std::move(hook));
    }

    void say(EventKind kind, EventLevel level, const std::string &text)
    {
        Event event = message(level, text);
        event.kind = kind;
        hooks.deliver(event);
    }

    void say(EventLevel level, const std::string &text) { say(EventKind::Message, level, text); }

    Hook recorder(const std::string &name)
    {
        return [this, name](const Event &event) { texts.push_back(name + event.text); };
    }

    Hooks hooks;
    Texts texts;
};

}

TEST_F(HookTest, Ids)
{
    EXPECT_EQ(listen(EventLevel::Debug, ignore), 1);
    const int second = listen(EventLevel::Debug, ignore);
    hooks.delete_hook(second);
    EXPECT_EQ(second, 2);
    EXPECT_EQ(listen(EventLevel::Debug, ignore), 3);
}

TEST_F(HookTest, Wanted)
{
    EXPECT_FALSE(hooks.wanted(EventKind::Message, EventLevel::Error));
    const int info = listen(EventLevel::Info, ignore);
    hooks.add_hook(EventKind::Invariant, EventLevel::Error, ignore);
    EXPECT_FALSE(hooks.wanted(EventKind::Message, EventLevel::Debug));
    EXPECT_TRUE(hooks.wanted(EventKind::Message, EventLevel::Info));
    EXPECT_FALSE(hooks.wanted(EventKind::Invariant, EventLevel::Warning));
    EXPECT_TRUE(hooks.wanted(EventKind::Invariant, EventLevel::Error));
    hooks.delete_hook(info);
    EXPECT_FALSE(hooks.wanted(EventKind::Message, EventLevel::Error));
}

TEST_F(HookTest, Order)
{
    listen(EventLevel::Debug, recorder("a "));
    listen(EventLevel::Warning, recorder("w "));
    hooks.add_hook(EventKind::Invariant, EventLevel::Debug, recorder("c "));
    listen(EventLevel::Debug, recorder("b "));
    say(EventLevel::Info, "one");
    say(EventLevel::Error, "two");
    say(EventKind::Invariant, EventLevel::Info, "three");
    EXPECT_EQ(texts, (Texts{"a one", "b one", "a two", "w two", "b two", "c three"}));
}

TEST_F(HookTest, Delivering)
{
    bool inside = false;
    listen(EventLevel::Debug, [&](const Event &) { inside = hooks.delivering(); });
    EXPECT_FALSE(hooks.delivering());
    say(EventLevel::Info, "one");
    EXPECT_TRUE(inside);
    EXPECT_FALSE(hooks.delivering());
}

TEST_F(HookTest, PeerRemoval)
{
    int peer = 0;
    listen(EventLevel::Debug, [&](const Event &) { hooks.delete_hook(peer); });
    peer = listen(EventLevel::Debug, recorder(""));
    say(EventLevel::Info, "one");
    EXPECT_TRUE(texts.empty());
    EXPECT_EQ(failure([&] { hooks.delete_hook(peer); }), "invalid argument: hook 2: unknown");
}

TEST_F(HookTest, Pruned)
{
    auto token = std::make_shared<int>(0);
    const std::weak_ptr<int> kept = token;
    bool alive = false;
    const int id = listen(EventLevel::Debug, Holder{std::move(token)});
    listen(EventLevel::Debug, [&](const Event &) { hooks.delete_hook(id); });
    listen(EventLevel::Debug, [&](const Event &) { alive = !kept.expired(); });
    say(EventLevel::Info, "one");
    EXPECT_TRUE(alive);
    EXPECT_TRUE(kept.expired());
}

TEST_F(HookTest, LateHook)
{
    listen(EventLevel::Debug, [this](const Event &) { listen(EventLevel::Debug, recorder("")); });
    say(EventLevel::Info, "one");
    say(EventLevel::Info, "two");
    EXPECT_EQ(texts, Texts{"two"});
}

TEST_F(HookTest, Throwing)
{
    listen(EventLevel::Debug, fail);
    listen(EventLevel::Debug, recorder(""));
    EXPECT_NO_THROW(say(EventLevel::Info, "one"));
    EXPECT_EQ(texts, Texts{"one"});
}

TEST_F(HookTest, Refusals)
{
    EXPECT_EQ(failure([&] { listen(EventLevel::Debug, nullptr); }),
              "invalid argument: hook: is empty");
    EXPECT_EQ(failure([&] { hooks.delete_hook(1); }), "invalid argument: hook 1: unknown");
    EXPECT_EQ(listen(EventLevel::Debug, ignore), 1);
}

TEST_F(HookTest, OtherThread)
{
    bool seen = true;
    listen(EventLevel::Debug, [&](const Event &) { seen = elsewhere(hooks); });
    say(EventLevel::Info, "one");
    EXPECT_FALSE(seen);
}

TEST_F(HookTest, Turns)
{
    Watch watch;
    std::latch start(kThreads + 1);
    std::vector<std::thread> threads;
    listen(EventLevel::Info, std::ref(watch));
    threads.emplace_back(churn, std::ref(hooks), std::ref(start));
    for (int i = 0; i < kThreads; ++i)
    {
        threads.emplace_back(chatter, std::ref(hooks), std::ref(start));
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(watch.overlaps(), 0);
    EXPECT_EQ(watch.seen(), kThreads * kRounds);
    EXPECT_FALSE(hooks.wanted(EventKind::Message, EventLevel::Debug));
}
