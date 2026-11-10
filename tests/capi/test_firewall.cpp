#include "capi/firewall.hpp"
#include "core/environment.hpp"
#include "core/events.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using biyos::EventKind;
using biyos::Level;

biyos_status status_after(const std::function<void()> &body)
{
    return biyos::capi::guard(body);
}

biyos_status status_after(biyos::Environment &env, const std::function<void()> &body)
{
    return biyos::capi::guard(&env, body);
}

void say(biyos::Environment &env, const std::string &text)
{
    const auto fill = [&text](biyos::Event &event) { event.text = text; };
    env.events().emit(EventKind::Message, Level::Info, fill);
}

void fail_once(biyos::Environment &env, int &calls)
{
    if (++calls == 1)
    {
        status_after(env, [] { throw std::runtime_error("inner"); });
    }
}

}

TEST(FirewallTest, NoThrow)
{
    bool ran = false;
    EXPECT_EQ(status_after([&] { ran = true; }), BIYOS_OK);
    EXPECT_TRUE(ran);
}

TEST(FirewallTest, InvalidArgument)
{
    const biyos_status status = status_after(
        [] { throw biyos::Error(biyos::ErrorKind::InvalidArgument, "argument 'n'", "negative"); });
    EXPECT_EQ(status, BIYOS_INVALID_ARGUMENT);
}

TEST(FirewallTest, NoMemory)
{
    const biyos_status status = status_after(
        [] { throw biyos::Error(biyos::ErrorKind::NoMemory, "workspace", "too large"); });
    EXPECT_EQ(status, BIYOS_NO_MEMORY);
}

TEST(FirewallTest, Internal)
{
    const biyos_status status = status_after(
        [] { throw biyos::Error(biyos::ErrorKind::Internal, "", "unreachable state"); });
    EXPECT_EQ(status, BIYOS_INTERNAL_ERROR);
}

TEST(FirewallTest, BadAlloc)
{
    EXPECT_EQ(status_after([] { throw std::bad_alloc(); }), BIYOS_NO_MEMORY);
}

TEST(FirewallTest, ForeignException)
{
    EXPECT_EQ(status_after([] { throw std::runtime_error("foreign"); }), BIYOS_INTERNAL_ERROR);
}

TEST(FirewallTest, UnknownException)
{
    EXPECT_EQ(status_after([] { throw 42; }), BIYOS_INTERNAL_ERROR);
}

TEST(FirewallTest, ErrorMessage)
{
    biyos::Environment env;
    const biyos::Error error(biyos::ErrorKind::InvalidArgument, "argument 'n'", "negative");
    EXPECT_EQ(status_after(env, [&] { throw error; }), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.last_error(), "invalid argument: argument 'n': negative");
}

TEST(FirewallTest, MemoryMessage)
{
    biyos::Environment env;
    EXPECT_EQ(status_after(env, [] { throw std::bad_alloc(); }), BIYOS_NO_MEMORY);
    EXPECT_EQ(env.last_error(), "out of memory");
}

TEST(FirewallTest, ForeignMessage)
{
    biyos::Environment env;
    EXPECT_EQ(status_after(env, [] { throw std::runtime_error("foreign"); }), BIYOS_INTERNAL_ERROR);
    EXPECT_EQ(env.last_error(), "internal error: foreign");
}

TEST(FirewallTest, UnknownMessage)
{
    biyos::Environment env;
    EXPECT_EQ(status_after(env, [] { throw 42; }), BIYOS_INTERNAL_ERROR);
    EXPECT_EQ(env.last_error(), "internal error: unknown exception");
}

TEST(FirewallTest, KeptMessage)
{
    biyos::Environment env;
    env.record_error("earlier");
    EXPECT_EQ(status_after(env, [] {}), BIYOS_OK);
    EXPECT_EQ(env.last_error(), "earlier");
}

TEST(FirewallTest, ErrorEvent)
{
    biyos::Environment env;
    std::vector<biyos::Event> kept;
    const auto keep = [&kept](const biyos::Event &event) { kept.push_back(event); };
    env.events().subscribe(EventKind::Message, Level::Error, keep);
    const biyos::Error error(biyos::ErrorKind::InvalidArgument, "argument 'n'", "negative");
    EXPECT_EQ(status_after(env, [] {}), BIYOS_OK);
    EXPECT_EQ(status_after(env, [&] { throw error; }), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(status_after(env, [] { throw std::bad_alloc(); }), BIYOS_NO_MEMORY);
    EXPECT_EQ(status_after(env, [] { throw 42; }), BIYOS_INTERNAL_ERROR);
    ASSERT_EQ(kept.size(), 3U);
    EXPECT_EQ(kept[0].kind, EventKind::Message);
    EXPECT_EQ(kept[0].level, Level::Error);
    EXPECT_EQ(kept[0].module, "capi");
    EXPECT_EQ(kept[0].text, "invalid argument: argument 'n': negative");
    EXPECT_EQ(kept[1].text, "out of memory");
    EXPECT_EQ(kept[2].text, "internal error: unknown exception");
}

TEST(FirewallTest, ThrowingSubscriber)
{
    biyos::Environment env;
    const auto fail = [](const biyos::Event &) { throw std::runtime_error("subscriber"); };
    env.events().subscribe(EventKind::Message, Level::Debug, fail);
    EXPECT_EQ(status_after(env, [&] { say(env, "info"); }), BIYOS_OK);
    EXPECT_EQ(env.last_error(), "");
    EXPECT_EQ(status_after(env, [] { throw std::bad_alloc(); }), BIYOS_NO_MEMORY);
    EXPECT_EQ(env.last_error(), "out of memory");
}

TEST(FirewallTest, ThrowingFill)
{
    biyos::Environment env;
    std::vector<biyos::Event> kept;
    const auto keep = [&kept](const biyos::Event &event) { kept.push_back(event); };
    env.events().subscribe(EventKind::Message, Level::Debug, keep);
    const auto fill = [](biyos::Event &) { throw std::bad_alloc(); };
    const auto body = [&] { env.events().emit(EventKind::Message, Level::Info, fill); };
    EXPECT_EQ(status_after(env, body), BIYOS_OK);
    EXPECT_EQ(env.last_error(), "");
    EXPECT_TRUE(kept.empty());
}

TEST(FirewallTest, NestedFailure)
{
    biyos::Environment env;
    int calls = 0;
    const auto nest = [&](const biyos::Event &) { fail_once(env, calls); };
    env.events().subscribe(EventKind::Message, Level::Error, nest);
    EXPECT_EQ(status_after(env, [] { throw std::runtime_error("outer"); }), BIYOS_INTERNAL_ERROR);
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(env.last_error(), "internal error: outer");
}
