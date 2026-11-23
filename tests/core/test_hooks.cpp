#include "core/error.hpp"
#include "core/event.hpp"
#include "core/hooks.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::EventKind;
using biyos::EventLevel;
using Texts = std::vector<std::string>;

std::string failure(const std::function<void()> &body)
{
    try
    {
        body();
    }
    catch (const biyos::Error &error)
    {
        return error.what();
    }
    return "no error";
}

void ignore(const biyos::Event &) {}

void fail(const biyos::Event &)
{
    throw std::runtime_error("hook");
}

struct Holder
{
    std::shared_ptr<int> held;

    void operator()(const biyos::Event &) const {}
};

class HookTest : public ::testing::Test
{
  protected:
    int listen(EventLevel level, biyos::Hook hook)
    {
        return hooks.add_hook(EventKind::Message, level, std::move(hook));
    }

    void say(EventKind kind, EventLevel level, const std::string &text)
    {
        const biyos::Event event{
            .kind = kind,
            .level = level,
            .time = {},
            .module = {},
            .text = text,
            .fields = {},
            .model = nullptr,
        };
        hooks.deliver(event);
    }

    void say(EventLevel level, const std::string &text) { say(EventKind::Message, level, text); }

    biyos::Hook recorder(const std::string &name)
    {
        return [this, name](const biyos::Event &event) { texts.push_back(name + event.text); };
    }

    biyos::Hooks hooks;
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
    hooks.add_hook(EventKind::Check, EventLevel::Error, ignore);
    EXPECT_FALSE(hooks.wanted(EventKind::Message, EventLevel::Debug));
    EXPECT_TRUE(hooks.wanted(EventKind::Message, EventLevel::Info));
    EXPECT_FALSE(hooks.wanted(EventKind::Check, EventLevel::Warning));
    EXPECT_TRUE(hooks.wanted(EventKind::Check, EventLevel::Error));
    hooks.delete_hook(info);
    EXPECT_FALSE(hooks.wanted(EventKind::Message, EventLevel::Error));
}

TEST_F(HookTest, Order)
{
    listen(EventLevel::Debug, recorder("a "));
    listen(EventLevel::Warning, recorder("w "));
    hooks.add_hook(EventKind::Check, EventLevel::Debug, recorder("c "));
    listen(EventLevel::Debug, recorder("b "));
    say(EventLevel::Info, "one");
    say(EventLevel::Error, "two");
    say(EventKind::Check, EventLevel::Info, "three");
    EXPECT_EQ(texts, (Texts{"a one", "b one", "a two", "w two", "b two", "c three"}));
}

TEST_F(HookTest, Delivering)
{
    bool inside = false;
    listen(EventLevel::Debug, [&](const biyos::Event &) { inside = hooks.delivering(); });
    EXPECT_FALSE(hooks.delivering());
    say(EventLevel::Info, "one");
    EXPECT_TRUE(inside);
    EXPECT_FALSE(hooks.delivering());
}

TEST_F(HookTest, PeerRemoval)
{
    int peer = 0;
    listen(EventLevel::Debug, [&](const biyos::Event &) { hooks.delete_hook(peer); });
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
    listen(EventLevel::Debug, [&](const biyos::Event &) { hooks.delete_hook(id); });
    listen(EventLevel::Debug, [&](const biyos::Event &) { alive = !kept.expired(); });
    say(EventLevel::Info, "one");
    EXPECT_TRUE(alive);
    EXPECT_TRUE(kept.expired());
}

TEST_F(HookTest, LateHook)
{
    listen(EventLevel::Debug,
           [this](const biyos::Event &) { listen(EventLevel::Debug, recorder("")); });
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
