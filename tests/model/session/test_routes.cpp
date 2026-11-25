#include "core/env.hpp"
#include "core/error.hpp"
#include "core/events/events.hpp"
#include "core/events/stream.hpp"
#include "model/model.hpp"
#include "model/session/session.hpp"
#include "watch.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <functional>
#include <latch>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{

using biyos::EventKind;
using biyos::EventLevel;
using biyos::Real;
using biyos::Session;
using biyos::test::Peek;
using biyos::test::Watch;
using Texts = std::vector<std::string>;

constexpr int kThreads = 4;
constexpr int kRounds = 200;

int unknown(const biyos::Request &, std::span<Real> out, void *)
{
    out[0] = std::nan("");
    return 0;
}

void cover(biyos::Model &model)
{
    const biyos::Variable free{.kind = biyos::VariableKind::Continuous, .lower = 0.0, .upper = 1.0};
    model.add_variables(std::span(&free, 1));
    const std::array<Real, 1> constants{0.0};
    const std::array<biyos::Offset, 2> starts{0, 0};
    model.add_functions(constants, starts, {});
    const std::array<biyos::Index, 1> covered{0};
    model.set_callback_function(model.add_callback(covered, nullptr), unknown);
}

void describe(biyos::Event &event)
{
    event.text = "removed rows";
}

void talk(const Session &model, std::latch &start)
{
    start.arrive_and_wait();
    for (int i = 0; i < kRounds; ++i)
    {
        model.stream().emit(EventKind::Message, EventLevel::Info, describe);
    }
}

void meet(std::latch &both, std::atomic<int> &met)
{
    both.arrive_and_wait();
    ++met;
}

void send(const Session &model)
{
    model.stream().emit(EventKind::Message, EventLevel::Info, describe);
}

class RouteTest : public ::testing::Test
{
  protected:
    int listen(EventLevel level, biyos::Hook hook)
    {
        return events.add_hook(EventKind::Message, level, std::move(hook));
    }

    void say(EventLevel level, const std::string &text)
    {
        const auto fill = [&text](biyos::Event &event) { event.text = text; };
        env.stream().emit(EventKind::Message, level, fill);
    }

    void tell(const Session &model, EventLevel level, const std::string &text)
    {
        const auto fill = [&text](biyos::Event &event) { event.text = text; };
        model.stream().emit(EventKind::Message, level, fill);
    }

    biyos::Hook recorder(const std::string &name)
    {
        return [this, name](const biyos::Event &event) { texts.push_back(name + event.text); };
    }

    biyos::Env env;
    biyos::Events &events = env.events();
    Texts texts;
};

}

TEST_F(RouteTest, Marked)
{
    const Session session(env);
    std::vector<const Session *> seen;
    const auto keep = [&seen](const biyos::Event &event) { seen.push_back(event.model); };
    events.add_hook(EventKind::Message, EventLevel::Info, keep);
    env.stream().emit(EventKind::Message, EventLevel::Info, describe);
    session.stream().emit(EventKind::Message, EventLevel::Info, describe);
    const std::vector<const Session *> expected{nullptr, &session};
    EXPECT_EQ(seen, expected);
}

TEST_F(RouteTest, ModelFirst)
{
    Session model(env);
    model.add_hook(EventKind::Message, EventLevel::Debug, recorder("m "));
    listen(EventLevel::Debug, recorder("e "));
    tell(model, EventLevel::Info, "one");
    say(EventLevel::Info, "two");
    EXPECT_EQ(texts, (Texts{"m one", "e one", "e two"}));
}

TEST_F(RouteTest, ModelOnly)
{
    Session model(env);
    int built = 0;
    const auto count = [&built](biyos::Event &) { ++built; };
    model.add_hook(EventKind::Message, EventLevel::Warning, recorder("m "));
    listen(EventLevel::Error, recorder("e "));
    model.stream().emit(EventKind::Message, EventLevel::Info, count);
    EXPECT_EQ(built, 0);
    model.stream().emit(EventKind::Message, EventLevel::Warning, count);
    EXPECT_EQ(built, 1);
    EXPECT_EQ(texts, Texts{"m "});
}

TEST_F(RouteTest, Delivering)
{
    Session model(env);
    Peek peek(events);
    bool inside = false;
    const auto check = [&](const biyos::Event &) { inside = events.delivering(); };
    const auto look = [&peek](const biyos::Event &event) { peek.hook(event); };
    const auto fill = [&peek](biyos::Event &event) { peek.fill(event); };
    model.add_hook(EventKind::Message, EventLevel::Debug, check);
    model.add_hook(EventKind::Message, EventLevel::Debug, look);
    model.stream().emit(EventKind::Message, EventLevel::Info, fill);
    EXPECT_TRUE(inside);
    EXPECT_FALSE(peek.delivering);
    EXPECT_FALSE(peek.filling);
    EXPECT_FALSE(events.delivering());
}

TEST_F(RouteTest, Turns)
{
    std::array<Session, 2> models{Session(env), Session(env)};
    std::array<Watch, 2> watches;
    Watch shared;
    std::latch start(kThreads);
    std::vector<std::thread> threads;
    events.add_hook(EventKind::Message, EventLevel::Info, std::ref(shared));
    for (std::size_t i = 0; i < models.size(); ++i)
    {
        models[i].add_hook(EventKind::Message, EventLevel::Info, std::ref(watches[i]));
    }
    for (int i = 0; i < kThreads; ++i)
    {
        const Session &model = models[static_cast<std::size_t>(i) % models.size()];
        threads.emplace_back(talk, std::cref(model), std::ref(start));
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(watches[0].overlaps() + watches[1].overlaps() + shared.overlaps(), 0);
    EXPECT_EQ(watches[0].seen(), kThreads / 2 * kRounds);
    EXPECT_EQ(watches[1].seen(), kThreads / 2 * kRounds);
    EXPECT_EQ(shared.seen(), kThreads * kRounds);
}

TEST_F(RouteTest, Beside)
{
    Session first(env);
    Session second(env);
    std::latch both(2);
    std::atomic<int> met{0};
    const auto meeting = [&both, &met](const biyos::Event &) { meet(both, met); };
    first.add_hook(EventKind::Message, EventLevel::Info, meeting);
    second.add_hook(EventKind::Message, EventLevel::Info, meeting);
    std::thread other(send, std::cref(second));
    send(first);
    other.join();
    EXPECT_EQ(met.load(), 2);
}

TEST_F(RouteTest, Invariant)
{
    Session model(env);
    cover(model.change());
    std::vector<const Session *> heard;
    const auto keep = [&heard](const biyos::Event &event) { heard.push_back(event.model); };
    model.add_hook(EventKind::Invariant, EventLevel::Error, keep);
    const std::array<Real, 1> x{0.5};
    std::array<Real, 1> f{};
    EXPECT_THROW(model.evaluation().values(model.lookup(), x, f), biyos::Error);
    EXPECT_EQ(heard, std::vector<const Session *>{&model});
    EXPECT_EQ(env.last_error().model, &model);
}
