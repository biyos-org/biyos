#include "core/error.hpp"
#include "core/events.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::EventKind;
using biyos::Level;
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
    ~Farewell() { events_.emit(EventKind::Message, Level::Debug, farewell); }
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

class EventTest : public ::testing::Test
{
  protected:
    int listen(Level lowest, biyos::Subscriber subscriber)
    {
        return events.subscribe(EventKind::Message, lowest, std::move(subscriber));
    }

    void say(Level level, const std::string &text)
    {
        events.emit(EventKind::Message, level, [&](biyos::Event &event) { event.text = text; });
    }

    biyos::Subscriber recorder(const std::string &name)
    {
        return [this, name](const biyos::Event &event) { texts.push_back(name + event.text); };
    }

    void rewire(int removed, const biyos::Event &event)
    {
        if (event.level == Level::Debug)
        {
            events.unsubscribe(removed);
            listen(Level::Info, recorder("e "));
        }
    }

    biyos::Events events;
    Texts texts;
};

}

TEST_F(EventTest, KindIds)
{
    EXPECT_EQ(biyos::find_kind(BIYOS_EVENT_MESSAGE), EventKind::Message);
    EXPECT_EQ(failure([] { biyos::find_kind(0); }), "invalid argument: event kind 0: unknown");
    EXPECT_EQ(failure([] { biyos::find_kind(2); }), "invalid argument: event kind 2: unknown");
}

TEST_F(EventTest, LevelIds)
{
    EXPECT_EQ(biyos::find_level(BIYOS_LEVEL_DEBUG), Level::Debug);
    EXPECT_EQ(biyos::find_level(BIYOS_LEVEL_INFO), Level::Info);
    EXPECT_EQ(biyos::find_level(BIYOS_LEVEL_WARNING), Level::Warning);
    EXPECT_EQ(biyos::find_level(BIYOS_LEVEL_ERROR), Level::Error);
    EXPECT_EQ(failure([] { biyos::find_level(0); }), "invalid argument: level 0: unknown");
    EXPECT_EQ(failure([] { biyos::find_level(5); }), "invalid argument: level 5: unknown");
}

TEST_F(EventTest, NoRecord)
{
    int built = 0;
    const auto count = [&built](biyos::Event &) { ++built; };
    events.emit(EventKind::Message, Level::Error, count);
    events.unsubscribe(listen(Level::Debug, ignore));
    events.emit(EventKind::Message, Level::Error, count);
    listen(Level::Error, ignore);
    events.emit(EventKind::Message, Level::Warning, count);
    EXPECT_EQ(built, 0);
    events.emit(EventKind::Message, Level::Error, count);
    EXPECT_EQ(built, 1);
}

TEST_F(EventTest, LevelFilter)
{
    listen(Level::Warning, recorder(""));
    say(Level::Debug, "debug");
    say(Level::Info, "info");
    say(Level::Warning, "warning");
    say(Level::Error, "error");
    EXPECT_EQ(texts, (Texts{"warning", "error"}));
    EXPECT_FALSE(events.wanted(EventKind::Message, Level::Info));
    EXPECT_TRUE(events.wanted(EventKind::Message, Level::Warning));
}

TEST_F(EventTest, LowestLevel)
{
    EXPECT_FALSE(events.wanted(EventKind::Message, Level::Error));
    const int info = listen(Level::Info, ignore);
    const int error = listen(Level::Error, ignore);
    EXPECT_FALSE(events.wanted(EventKind::Message, Level::Debug));
    EXPECT_TRUE(events.wanted(EventKind::Message, Level::Info));
    events.unsubscribe(info);
    EXPECT_FALSE(events.wanted(EventKind::Message, Level::Warning));
    EXPECT_TRUE(events.wanted(EventKind::Message, Level::Error));
    events.unsubscribe(error);
    EXPECT_FALSE(events.wanted(EventKind::Message, Level::Error));
}

TEST_F(EventTest, Delivering)
{
    bool inside = false;
    listen(Level::Debug, [&](const biyos::Event &) { inside = events.delivering(); });
    EXPECT_FALSE(events.delivering());
    say(Level::Info, "one");
    EXPECT_TRUE(inside);
    EXPECT_FALSE(events.delivering());
}

TEST_F(EventTest, Order)
{
    listen(Level::Debug, recorder("a "));
    const int b = listen(Level::Debug, recorder("b "));
    listen(Level::Debug, recorder("c "));
    say(Level::Info, "one");
    events.unsubscribe(b);
    listen(Level::Debug, recorder("b "));
    say(Level::Info, "two");
    EXPECT_EQ(texts, (Texts{"a one", "b one", "c one", "a two", "c two", "b two"}));
}

TEST_F(EventTest, Contents)
{
    std::vector<biyos::Event> kept;
    listen(Level::Debug, [&kept](const biyos::Event &event) { kept.push_back(event); });
    events.emit(EventKind::Message, Level::Warning, describe);
    ASSERT_EQ(kept.size(), 1U);
    const biyos::Event &event = kept.front();
    EXPECT_EQ(event.kind, EventKind::Message);
    EXPECT_EQ(event.level, Level::Warning);
    EXPECT_EQ(event.module, "presolve");
    EXPECT_EQ(event.text, "removed rows");
    ASSERT_EQ(event.fields.size(), 1U);
    EXPECT_EQ(event.fields.front().name, "rows");
    EXPECT_EQ(event.fields.front().value, "3");
}

TEST_F(EventTest, Ids)
{
    const int first = listen(Level::Debug, ignore);
    const int second = listen(Level::Debug, ignore);
    events.unsubscribe(second);
    const int third = listen(Level::Debug, ignore);
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 2);
    EXPECT_EQ(third, 3);
}

TEST_F(EventTest, EmptySubscriber)
{
    EXPECT_EQ(failure([&] { listen(Level::Debug, nullptr); }),
              "invalid argument: subscriber: is empty");
    EXPECT_FALSE(events.wanted(EventKind::Message, Level::Error));
    EXPECT_EQ(listen(Level::Debug, ignore), 1);
}

TEST_F(EventTest, UnknownId)
{
    const int id = listen(Level::Debug, ignore);
    events.unsubscribe(id);
    EXPECT_EQ(failure([&] { events.unsubscribe(id); }),
              "invalid argument: subscription 1: unknown");
    EXPECT_EQ(failure([&] { events.unsubscribe(7); }), "invalid argument: subscription 7: unknown");
}

TEST_F(EventTest, SelfRemoval)
{
    int id = 0;
    id = listen(Level::Debug, [&](const biyos::Event &) { events.unsubscribe(id); });
    say(Level::Info, "one");
    EXPECT_FALSE(events.wanted(EventKind::Message, Level::Error));
    EXPECT_NO_THROW(say(Level::Info, "two"));
}

TEST_F(EventTest, PeerRemoval)
{
    int peer = 0;
    listen(Level::Debug, [&](const biyos::Event &) { events.unsubscribe(peer); });
    peer = listen(Level::Debug, recorder(""));
    say(Level::Info, "one");
    EXPECT_TRUE(texts.empty());
    EXPECT_EQ(failure([&] { events.unsubscribe(peer); }),
              "invalid argument: subscription 2: unknown");
}

TEST_F(EventTest, NestedRemoval)
{
    const int x = listen(Level::Info, recorder("x "));
    listen(Level::Info, [this](const biyos::Event &) { say(Level::Debug, "inner"); });
    listen(Level::Info, recorder("c "));
    listen(Level::Info, recorder("d "));
    listen(Level::Debug, [this, x](const biyos::Event &event) { rewire(x, event); });
    say(Level::Info, "outer");
    EXPECT_EQ(texts, (Texts{"x outer", "c outer", "d outer"}));
}

TEST_F(EventTest, ErasedFarewell)
{
    const int id = listen(Level::Debug, Parting(events, texts));
    listen(Level::Debug, recorder("r "));
    events.unsubscribe(id);
    EXPECT_EQ(texts, Texts{"r farewell"});
}

TEST_F(EventTest, PrunedFarewell)
{
    const int id = listen(Level::Debug, Parting(events, texts));
    listen(Level::Debug, recorder("r "));
    listen(Level::Info, [this, id](const biyos::Event &) { events.unsubscribe(id); });
    say(Level::Info, "one");
    EXPECT_EQ(texts, (Texts{"p one", "r one", "r farewell"}));
}

TEST_F(EventTest, LateSubscriber)
{
    listen(Level::Debug, [this](const biyos::Event &) { listen(Level::Debug, recorder("")); });
    say(Level::Info, "one");
    say(Level::Info, "two");
    EXPECT_EQ(texts, Texts{"two"});
}

TEST_F(EventTest, SubscriberThrows)
{
    const auto fail = [](const biyos::Event &) { throw std::runtime_error("subscriber"); };
    listen(Level::Debug, fail);
    listen(Level::Debug, recorder(""));
    EXPECT_NO_THROW(say(Level::Info, "one"));
    EXPECT_EQ(texts, Texts{"one"});
}

TEST_F(EventTest, ThrowingFill)
{
    listen(Level::Debug, recorder(""));
    const auto fill = [](biyos::Event &) { throw std::bad_alloc(); };
    EXPECT_NO_THROW(events.emit(EventKind::Message, Level::Info, fill));
    say(Level::Info, "after");
    EXPECT_EQ(texts, Texts{"after"});
}
