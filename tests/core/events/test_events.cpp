#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/events/events.hpp"
#include "core/events/hooks.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <functional>
#include <latch>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{

using biyos::EventKind;
using biyos::EventLevel;
using Texts = std::vector<std::string>;

constexpr int kThreads = 4;
constexpr int kRounds = 200;

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

void describe(biyos::Event &event)
{
    event.module = "presolve";
    event.text = "removed rows";
    event.fields.push_back(biyos::Field{.name = "rows", .value = "3"});
}

void farewell(biyos::Event &event)
{
    event.text = "farewell";
}

class Farewell
{
  public:
    explicit Farewell(biyos::Events &events) : events_(events) {}
    ~Farewell() { events_.emit(EventKind::Message, EventLevel::Debug, farewell); }
    Farewell(const Farewell &) = delete;
    Farewell &operator=(const Farewell &) = delete;

  private:
    biyos::Events &events_;
};

class Parting
{
  public:
    Parting(biyos::Events &events, Texts &texts)
        : farewell_(std::make_shared<Farewell>(events)), texts_(&texts)
    {
    }

    void operator()(const biyos::Event &event) const { texts_->push_back("p " + event.text); }

  private:
    std::shared_ptr<Farewell> farewell_;
    Texts *texts_;
};

class Watch
{
  public:
    void operator()(const biyos::Event &)
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

void chatter(biyos::Events &events, std::latch &start)
{
    start.arrive_and_wait();
    for (int i = 0; i < kRounds; ++i)
    {
        events.emit(EventKind::Message, EventLevel::Info, describe);
    }
}

void churn(biyos::Events &events, std::latch &start)
{
    start.arrive_and_wait();
    for (int i = 0; i < kRounds; ++i)
    {
        events.delete_hook(events.add_hook(EventKind::Message, EventLevel::Debug, ignore));
    }
}

void talk(biyos::Events &events, biyos::Hooks &model, std::latch &start)
{
    start.arrive_and_wait();
    for (int i = 0; i < kRounds; ++i)
    {
        events.emit(EventKind::Message, EventLevel::Info, describe, &model);
    }
}

void meet(std::latch &both, std::atomic<int> &met)
{
    both.arrive_and_wait();
    ++met;
}

void send(biyos::Events &events, biyos::Hooks &model)
{
    events.emit(EventKind::Message, EventLevel::Info, describe, &model);
}

using Read = bool (biyos::Events::*)() const noexcept;

bool elsewhere(const biyos::Events &events, Read read)
{
    bool seen = true;
    std::thread([&] { seen = (events.*read)(); }).join();
    return seen;
}

class Peek
{
  public:
    explicit Peek(const biyos::Events &events) : events_(events) {}

    void hook(const biyos::Event &) { delivering = elsewhere(events_, &biyos::Events::delivering); }
    void fill(biyos::Event &) { filling = elsewhere(events_, &biyos::Events::filling); }

    bool delivering = true;
    bool filling = true;

  private:
    const biyos::Events &events_;
};

class EventTest : public ::testing::Test
{
  protected:
    int listen(EventLevel level, biyos::Hook hook)
    {
        return events.add_hook(EventKind::Message, level, std::move(hook));
    }

    void say(EventLevel level, const std::string &text)
    {
        events.emit(EventKind::Message, level, [&](biyos::Event &event) { event.text = text; });
    }

    void tell(biyos::Hooks &model, EventLevel level, const std::string &text)
    {
        const auto fill = [&text](biyos::Event &event) { event.text = text; };
        events.emit(EventKind::Message, level, fill, &model);
    }

    biyos::Hook recorder(const std::string &name)
    {
        return [this, name](const biyos::Event &event) { texts.push_back(name + event.text); };
    }

    void rewire(int removed, const biyos::Event &event)
    {
        if (event.level == EventLevel::Debug)
        {
            events.delete_hook(removed);
            listen(EventLevel::Info, recorder("e "));
        }
    }

    biyos::FakeClock clock;
    biyos::Events events{clock};
    Texts texts;
};

}

TEST_F(EventTest, KindIds)
{
    EXPECT_EQ(biyos::find_kind(BIYOS_EVTKND_MSG), EventKind::Message);
    EXPECT_EQ(biyos::find_kind(BIYOS_EVTKND_INV), EventKind::Invariant);
    EXPECT_EQ(biyos::find_kind(BIYOS_EVTKND_SPAN), EventKind::Span);
    EXPECT_EQ(failure([] { biyos::find_kind(0); }), "invalid argument: event kind 0: unknown");
    EXPECT_EQ(failure([] { biyos::find_kind(4); }), "invalid argument: event kind 4: unknown");
}

TEST_F(EventTest, LevelIds)
{
    EXPECT_EQ(biyos::find_level(BIYOS_EVTLVL_DBG), EventLevel::Debug);
    EXPECT_EQ(biyos::find_level(BIYOS_EVTLVL_INFO), EventLevel::Info);
    EXPECT_EQ(biyos::find_level(BIYOS_EVTLVL_WARN), EventLevel::Warning);
    EXPECT_EQ(biyos::find_level(BIYOS_EVTLVL_ERR), EventLevel::Error);
    EXPECT_EQ(failure([] { biyos::find_level(0); }), "invalid argument: level 0: unknown");
    EXPECT_EQ(failure([] { biyos::find_level(5); }), "invalid argument: level 5: unknown");
}

TEST_F(EventTest, Names)
{
    EXPECT_EQ(biyos::name_of(EventKind::Message), "message");
    EXPECT_EQ(biyos::name_of(EventKind::Invariant), "invariant");
    EXPECT_EQ(biyos::name_of(EventKind::Span), "span");
    EXPECT_EQ(biyos::name_of(EventLevel::Debug), "debug");
    EXPECT_EQ(biyos::name_of(EventLevel::Info), "info");
    EXPECT_EQ(biyos::name_of(EventLevel::Warning), "warning");
    EXPECT_EQ(biyos::name_of(EventLevel::Error), "error");
}

TEST_F(EventTest, NoRecord)
{
    int built = 0;
    const auto count = [&built](biyos::Event &) { ++built; };
    events.emit(EventKind::Message, EventLevel::Error, count);
    events.delete_hook(listen(EventLevel::Debug, ignore));
    events.emit(EventKind::Message, EventLevel::Error, count);
    listen(EventLevel::Error, ignore);
    events.emit(EventKind::Message, EventLevel::Warning, count);
    EXPECT_EQ(built, 0);
    events.emit(EventKind::Message, EventLevel::Error, count);
    EXPECT_EQ(built, 1);
}

TEST_F(EventTest, LevelFilter)
{
    listen(EventLevel::Warning, recorder(""));
    say(EventLevel::Debug, "debug");
    say(EventLevel::Info, "info");
    say(EventLevel::Warning, "warning");
    say(EventLevel::Error, "error");
    EXPECT_EQ(texts, (Texts{"warning", "error"}));
    EXPECT_FALSE(events.wanted(EventKind::Message, EventLevel::Info));
    EXPECT_TRUE(events.wanted(EventKind::Message, EventLevel::Warning));
}

TEST_F(EventTest, KindFilter)
{
    const auto two = [](biyos::Event &event) { event.text = "two"; };
    listen(EventLevel::Debug, recorder("m "));
    events.add_hook(EventKind::Invariant, EventLevel::Error, recorder("c "));
    EXPECT_TRUE(events.wanted(EventKind::Message, EventLevel::Debug));
    EXPECT_FALSE(events.wanted(EventKind::Invariant, EventLevel::Warning));
    say(EventLevel::Error, "one");
    events.emit(EventKind::Invariant, EventLevel::Error, two);
    EXPECT_EQ(texts, (Texts{"m one", "c two"}));
}

TEST_F(EventTest, LowestLevel)
{
    EXPECT_FALSE(events.wanted(EventKind::Message, EventLevel::Error));
    const int info = listen(EventLevel::Info, ignore);
    const int error = listen(EventLevel::Error, ignore);
    EXPECT_FALSE(events.wanted(EventKind::Message, EventLevel::Debug));
    EXPECT_TRUE(events.wanted(EventKind::Message, EventLevel::Info));
    events.delete_hook(info);
    EXPECT_FALSE(events.wanted(EventKind::Message, EventLevel::Warning));
    EXPECT_TRUE(events.wanted(EventKind::Message, EventLevel::Error));
    events.delete_hook(error);
    EXPECT_FALSE(events.wanted(EventKind::Message, EventLevel::Error));
}

TEST_F(EventTest, Delivering)
{
    bool inside = false;
    listen(EventLevel::Debug, [&](const biyos::Event &) { inside = events.delivering(); });
    EXPECT_FALSE(events.delivering());
    say(EventLevel::Info, "one");
    EXPECT_TRUE(inside);
    EXPECT_FALSE(events.delivering());
}

TEST_F(EventTest, Filling)
{
    bool filled = false;
    bool delivered = true;
    const auto fill = [&](biyos::Event &) { filled = events.filling(); };
    const auto fail = [](biyos::Event &) { throw std::runtime_error("fill"); };
    listen(EventLevel::Debug, [&](const biyos::Event &) { delivered = events.filling(); });
    EXPECT_FALSE(events.filling());
    events.emit(EventKind::Message, EventLevel::Info, fill);
    EXPECT_TRUE(filled);
    EXPECT_FALSE(delivered);
    events.emit(EventKind::Message, EventLevel::Info, fail);
    EXPECT_FALSE(events.filling());
}

TEST_F(EventTest, Order)
{
    listen(EventLevel::Debug, recorder("a "));
    const int b = listen(EventLevel::Debug, recorder("b "));
    listen(EventLevel::Debug, recorder("c "));
    say(EventLevel::Info, "one");
    events.delete_hook(b);
    listen(EventLevel::Debug, recorder("b "));
    say(EventLevel::Info, "two");
    EXPECT_EQ(texts, (Texts{"a one", "b one", "c one", "a two", "c two", "b two"}));
}

TEST_F(EventTest, Contents)
{
    std::vector<biyos::Event> kept;
    listen(EventLevel::Debug, [&kept](const biyos::Event &event) { kept.push_back(event); });
    events.emit(EventKind::Message, EventLevel::Warning, describe);
    ASSERT_EQ(kept.size(), 1U);
    const biyos::Event &event = kept.front();
    EXPECT_EQ(event.kind, EventKind::Message);
    EXPECT_EQ(event.level, EventLevel::Warning);
    EXPECT_EQ(event.module, "presolve");
    EXPECT_EQ(event.text, "removed rows");
    ASSERT_EQ(event.fields.size(), 1U);
    EXPECT_EQ(event.fields.front().name, "rows");
    EXPECT_EQ(event.fields.front().value, "3");
}

TEST_F(EventTest, Ids)
{
    const int first = listen(EventLevel::Debug, ignore);
    const int second = listen(EventLevel::Debug, ignore);
    events.delete_hook(second);
    const int third = listen(EventLevel::Debug, ignore);
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);
    EXPECT_EQ(third, 3);
}

TEST_F(EventTest, EmptyHook)
{
    EXPECT_EQ(failure([&] { listen(EventLevel::Debug, nullptr); }),
              "invalid argument: hook: is empty");
    EXPECT_FALSE(events.wanted(EventKind::Message, EventLevel::Error));
    EXPECT_EQ(listen(EventLevel::Debug, ignore), 1);
}

TEST_F(EventTest, UnknownId)
{
    const int id = listen(EventLevel::Debug, ignore);
    events.delete_hook(id);
    EXPECT_EQ(failure([&] { events.delete_hook(id); }), "invalid argument: hook 1: unknown");
    EXPECT_EQ(failure([&] { events.delete_hook(7); }), "invalid argument: hook 7: unknown");
}

TEST_F(EventTest, SelfRemoval)
{
    int id = 0;
    id = listen(EventLevel::Debug, [&](const biyos::Event &) { events.delete_hook(id); });
    say(EventLevel::Info, "one");
    EXPECT_FALSE(events.wanted(EventKind::Message, EventLevel::Error));
    EXPECT_NO_THROW(say(EventLevel::Info, "two"));
}

TEST_F(EventTest, PeerRemoval)
{
    int peer = 0;
    listen(EventLevel::Debug, [&](const biyos::Event &) { events.delete_hook(peer); });
    peer = listen(EventLevel::Debug, recorder(""));
    say(EventLevel::Info, "one");
    EXPECT_TRUE(texts.empty());
    EXPECT_EQ(failure([&] { events.delete_hook(peer); }), "invalid argument: hook 2: unknown");
}

TEST_F(EventTest, NestedRemoval)
{
    const int x = listen(EventLevel::Info, recorder("x "));
    listen(EventLevel::Info, [this](const biyos::Event &) { say(EventLevel::Debug, "inner"); });
    listen(EventLevel::Info, recorder("c "));
    listen(EventLevel::Info, recorder("d "));
    listen(EventLevel::Debug, [this, x](const biyos::Event &event) { rewire(x, event); });
    say(EventLevel::Info, "outer");
    EXPECT_EQ(texts, (Texts{"x outer", "c outer", "d outer"}));
}

TEST_F(EventTest, ErasedFarewell)
{
    const int id = listen(EventLevel::Debug, Parting(events, texts));
    listen(EventLevel::Debug, recorder("r "));
    events.delete_hook(id);
    EXPECT_EQ(texts, Texts{"r farewell"});
}

TEST_F(EventTest, PrunedFarewell)
{
    const int id = listen(EventLevel::Debug, Parting(events, texts));
    listen(EventLevel::Debug, recorder("r "));
    listen(EventLevel::Info, [this, id](const biyos::Event &) { events.delete_hook(id); });
    say(EventLevel::Info, "one");
    EXPECT_EQ(texts, (Texts{"p one", "r one", "r farewell"}));
}

TEST_F(EventTest, LateHook)
{
    listen(EventLevel::Debug,
           [this](const biyos::Event &) { listen(EventLevel::Debug, recorder("")); });
    say(EventLevel::Info, "one");
    say(EventLevel::Info, "two");
    EXPECT_EQ(texts, Texts{"two"});
}

TEST_F(EventTest, HookThrows)
{
    const auto fail = [](const biyos::Event &) { throw std::runtime_error("hook"); };
    listen(EventLevel::Debug, fail);
    listen(EventLevel::Debug, recorder(""));
    EXPECT_NO_THROW(say(EventLevel::Info, "one"));
    EXPECT_EQ(texts, Texts{"one"});
}

TEST_F(EventTest, ThrowingFill)
{
    listen(EventLevel::Debug, recorder(""));
    const auto fill = [](biyos::Event &) { throw std::bad_alloc(); };
    EXPECT_NO_THROW(events.emit(EventKind::Message, EventLevel::Info, fill));
    say(EventLevel::Info, "after");
    EXPECT_EQ(texts, Texts{"after"});
}

TEST_F(EventTest, Model)
{
    const int address = 0;
    const auto *model = reinterpret_cast<const BIYOS_model *>(&address);
    std::vector<const BIYOS_model *> seen;
    const auto keep = [&seen](const biyos::Event &event) { seen.push_back(event.model); };
    const auto named = [model](biyos::Event &event) { event.model = model; };
    events.add_hook(EventKind::Message, EventLevel::Info, keep);
    events.emit(EventKind::Message, EventLevel::Info, describe);
    events.emit(EventKind::Message, EventLevel::Info, named);
    const std::vector<const BIYOS_model *> expected{nullptr, model};
    EXPECT_EQ(seen, expected);
}

TEST_F(EventTest, Turns)
{
    Watch watch;
    std::latch start(kThreads);
    std::vector<std::thread> threads;
    events.add_hook(EventKind::Message, EventLevel::Info, std::ref(watch));
    for (int i = 0; i < kThreads; ++i)
    {
        threads.emplace_back(chatter, std::ref(events), std::ref(start));
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(watch.overlaps(), 0);
    EXPECT_EQ(watch.seen(), kThreads * kRounds);
}

TEST_F(EventTest, Churn)
{
    Watch watch;
    std::latch start(kThreads + 1);
    std::vector<std::thread> threads;
    events.add_hook(EventKind::Message, EventLevel::Info, std::ref(watch));
    threads.emplace_back(churn, std::ref(events), std::ref(start));
    for (int i = 0; i < kThreads; ++i)
    {
        threads.emplace_back(chatter, std::ref(events), std::ref(start));
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(watch.overlaps(), 0);
    EXPECT_EQ(watch.seen(), kThreads * kRounds);
    EXPECT_FALSE(events.wanted(EventKind::Message, EventLevel::Debug));
}

TEST_F(EventTest, OtherThread)
{
    Peek peek(events);
    listen(EventLevel::Debug, [&peek](const biyos::Event &event) { peek.hook(event); });
    events.emit(EventKind::Message,
                EventLevel::Info,
                [&peek](biyos::Event &event) { peek.fill(event); });
    EXPECT_FALSE(peek.delivering);
    EXPECT_FALSE(peek.filling);
}

TEST_F(EventTest, ModelFirst)
{
    biyos::Hooks model;
    model.add_hook(EventKind::Message, EventLevel::Debug, recorder("m "));
    listen(EventLevel::Debug, recorder("e "));
    tell(model, EventLevel::Info, "one");
    say(EventLevel::Info, "two");
    EXPECT_EQ(texts, (Texts{"m one", "e one", "e two"}));
}

TEST_F(EventTest, ModelOnly)
{
    biyos::Hooks model;
    int built = 0;
    const auto count = [&built](biyos::Event &) { ++built; };
    model.add_hook(EventKind::Message, EventLevel::Warning, recorder("m "));
    listen(EventLevel::Error, recorder("e "));
    events.emit(EventKind::Message, EventLevel::Info, count, &model);
    EXPECT_EQ(built, 0);
    events.emit(EventKind::Message, EventLevel::Warning, count, &model);
    EXPECT_EQ(built, 1);
    EXPECT_EQ(texts, Texts{"m "});
}

TEST_F(EventTest, ModelDelivering)
{
    biyos::Hooks model;
    Peek peek(events);
    bool inside = false;
    const auto check = [&](const biyos::Event &) { inside = events.delivering(); };
    const auto look = [&peek](const biyos::Event &event) { peek.hook(event); };
    const auto fill = [&peek](biyos::Event &event) { peek.fill(event); };
    model.add_hook(EventKind::Message, EventLevel::Debug, check);
    model.add_hook(EventKind::Message, EventLevel::Debug, look);
    events.emit(EventKind::Message, EventLevel::Info, fill, &model);
    EXPECT_TRUE(inside);
    EXPECT_FALSE(peek.delivering);
    EXPECT_FALSE(peek.filling);
    EXPECT_FALSE(events.delivering());
}

TEST_F(EventTest, ModelTurns)
{
    std::array<biyos::Hooks, 2> models;
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
        biyos::Hooks &model = models[static_cast<std::size_t>(i) % models.size()];
        threads.emplace_back(talk, std::ref(events), std::ref(model), std::ref(start));
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

TEST_F(EventTest, ModelsBeside)
{
    biyos::Hooks first;
    biyos::Hooks second;
    std::latch both(2);
    std::atomic<int> met{0};
    const auto meeting = [&both, &met](const biyos::Event &) { meet(both, met); };
    first.add_hook(EventKind::Message, EventLevel::Info, meeting);
    second.add_hook(EventKind::Message, EventLevel::Info, meeting);
    std::thread other(send, std::ref(events), std::ref(second));
    send(events, first);
    other.join();
    EXPECT_EQ(met.load(), 2);
}
