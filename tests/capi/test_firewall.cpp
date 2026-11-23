#include "capi/firewall.hpp"
#include "core/env.hpp"
#include "core/events.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using biyos::EventKind;
using biyos::EventLevel;

BIYOS_status status_after(const std::function<void()> &body)
{
    return biyos::capi::guard(body);
}

BIYOS_status status_after(biyos::Env &env, const std::function<void()> &body)
{
    return biyos::capi::guard(&env, nullptr, body);
}

void say(biyos::Env &env, const std::string &text)
{
    const auto fill = [&text](biyos::Event &event) { event.text = text; };
    env.events().emit(EventKind::Message, EventLevel::Info, fill);
}

void fail_inside(biyos::Env &env, int &calls)
{
    const biyos::Env::ErrorScope scope(env);
    ++calls;
    status_after(env, [] { throw std::runtime_error("inner"); });
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
    const BIYOS_status status = status_after(
        [] { throw biyos::Error(biyos::ErrorKind::InvalidArgument, "argument 'n'", "negative"); });
    EXPECT_EQ(status, BIYOS_ERR_BADARG);
}

TEST(FirewallTest, NoMemory)
{
    const BIYOS_status status = status_after(
        [] { throw biyos::Error(biyos::ErrorKind::NoMemory, "workspace", "too large"); });
    EXPECT_EQ(status, BIYOS_ERR_NOMEMORY);
}

TEST(FirewallTest, Internal)
{
    const BIYOS_status status = status_after(
        [] { throw biyos::Error(biyos::ErrorKind::Internal, "", "unreachable state"); });
    EXPECT_EQ(status, BIYOS_ERR_INTERNAL);
}

TEST(FirewallTest, BadAlloc)
{
    EXPECT_EQ(status_after([] { throw std::bad_alloc(); }), BIYOS_ERR_NOMEMORY);
}

TEST(FirewallTest, ForeignException)
{
    EXPECT_EQ(status_after([] { throw std::runtime_error("foreign"); }), BIYOS_ERR_INTERNAL);
}

TEST(FirewallTest, UnknownException)
{
    EXPECT_EQ(status_after([] { throw 42; }), BIYOS_ERR_INTERNAL);
}

TEST(FirewallTest, ErrorMessage)
{
    biyos::Env env;
    const biyos::Error error(biyos::ErrorKind::InvalidArgument, "argument 'n'", "negative");
    EXPECT_EQ(status_after(env, [&] { throw error; }), BIYOS_ERR_BADARG);
    EXPECT_STREQ(env.last_error().text, "invalid argument: argument 'n': negative");
}

TEST(FirewallTest, FailureModel)
{
    const int address = 0;
    const auto *model = reinterpret_cast<const BIYOS_model *>(&address);
    biyos::Env env;
    std::vector<const BIYOS_model *> seen;
    const auto keep = [&seen](const biyos::Event &event) { seen.push_back(event.model); };
    env.events().add_hook(EventKind::Message, EventLevel::Error, keep);
    EXPECT_EQ(biyos::capi::guard(&env, model, [] { throw std::bad_alloc(); }), BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.last_error().model, model);
    EXPECT_EQ(env.last_error().status, BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(status_after(env, [] { throw std::bad_alloc(); }), BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.last_error().model, nullptr);
    EXPECT_EQ(seen, (std::vector<const BIYOS_model *>{model, nullptr}));
}

TEST(FirewallTest, MemoryMessage)
{
    biyos::Env env;
    EXPECT_EQ(status_after(env, [] { throw std::bad_alloc(); }), BIYOS_ERR_NOMEMORY);
    EXPECT_STREQ(env.last_error().text, "out of memory");
}

TEST(FirewallTest, ForeignMessage)
{
    biyos::Env env;
    EXPECT_EQ(status_after(env, [] { throw std::runtime_error("foreign"); }), BIYOS_ERR_INTERNAL);
    EXPECT_STREQ(env.last_error().text, "internal error: foreign");
}

TEST(FirewallTest, UnknownMessage)
{
    biyos::Env env;
    EXPECT_EQ(status_after(env, [] { throw 42; }), BIYOS_ERR_INTERNAL);
    EXPECT_STREQ(env.last_error().text, "internal error: unknown exception");
}

TEST(FirewallTest, KeptMessage)
{
    biyos::Env env;
    env.record_error(biyos::ErrorKind::InvalidArgument, "earlier");
    EXPECT_EQ(status_after(env, [] {}), BIYOS_OK);
    EXPECT_STREQ(env.last_error().text, "earlier");
}

TEST(FirewallTest, ErrorEvent)
{
    biyos::Env env;
    std::vector<biyos::Event> kept;
    const auto keep = [&kept](const biyos::Event &event) { kept.push_back(event); };
    env.events().add_hook(EventKind::Message, EventLevel::Error, keep);
    const biyos::Error error(biyos::ErrorKind::InvalidArgument, "argument 'n'", "negative");
    EXPECT_EQ(status_after(env, [] {}), BIYOS_OK);
    EXPECT_EQ(status_after(env, [&] { throw error; }), BIYOS_ERR_BADARG);
    EXPECT_EQ(status_after(env, [] { throw std::bad_alloc(); }), BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(status_after(env, [] { throw 42; }), BIYOS_ERR_INTERNAL);
    ASSERT_EQ(kept.size(), 3U);
    EXPECT_EQ(kept[0].kind, EventKind::Message);
    EXPECT_EQ(kept[0].level, EventLevel::Error);
    EXPECT_EQ(kept[0].module, "capi");
    EXPECT_EQ(kept[0].text, "invalid argument: argument 'n': negative");
    EXPECT_EQ(kept[1].text, "out of memory");
    EXPECT_EQ(kept[2].text, "internal error: unknown exception");
}

TEST(FirewallTest, ErrorCode)
{
    biyos::Env env;
    std::vector<biyos::Event> kept;
    const auto keep = [&kept](const biyos::Event &event) { kept.push_back(event); };
    env.events().add_hook(EventKind::Message, EventLevel::Error, keep);
    const biyos::Error error(biyos::ErrorKind::InvalidArgument, "argument 'n'", "negative");
    EXPECT_EQ(status_after(env, [&] { throw error; }), BIYOS_ERR_BADARG);
    EXPECT_EQ(status_after(env, [] { throw std::bad_alloc(); }), BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(status_after(env, [] { throw 42; }), BIYOS_ERR_INTERNAL);
    ASSERT_EQ(kept.size(), 3U);
    const char *codes[] = {"-100", "-400", "-500"};
    for (std::size_t i = 0; i < kept.size(); ++i)
    {
        ASSERT_EQ(kept[i].fields.size(), 1U);
        EXPECT_EQ(kept[i].fields[0].name, "code");
        EXPECT_EQ(kept[i].fields[0].value, codes[i]);
    }
}

TEST(FirewallTest, ThrowingHook)
{
    biyos::Env env;
    const auto fail = [](const biyos::Event &) { throw std::runtime_error("hook"); };
    env.events().add_hook(EventKind::Message, EventLevel::Debug, fail);
    EXPECT_EQ(status_after(env, [&] { say(env, "info"); }), BIYOS_OK);
    EXPECT_STREQ(env.last_error().text, "");
    EXPECT_EQ(status_after(env, [] { throw std::bad_alloc(); }), BIYOS_ERR_NOMEMORY);
    EXPECT_STREQ(env.last_error().text, "out of memory");
}

TEST(FirewallTest, ThrowingFill)
{
    biyos::Env env;
    std::vector<biyos::Event> kept;
    const auto keep = [&kept](const biyos::Event &event) { kept.push_back(event); };
    env.events().add_hook(EventKind::Message, EventLevel::Debug, keep);
    const auto fill = [](biyos::Event &) { throw std::bad_alloc(); };
    const auto body = [&] { env.events().emit(EventKind::Message, EventLevel::Info, fill); };
    EXPECT_EQ(status_after(env, body), BIYOS_OK);
    EXPECT_STREQ(env.last_error().text, "");
    EXPECT_TRUE(kept.empty());
}

TEST(FirewallTest, CurrentMessage)
{
    biyos::Env env;
    env.record_error(biyos::ErrorKind::InvalidArgument, "earlier");
    std::string seen;
    const auto read = [&](const biyos::Event &) { seen = env.last_error().text; };
    env.events().add_hook(EventKind::Message, EventLevel::Error, read);
    EXPECT_EQ(status_after(env, [] { throw std::runtime_error("now"); }), BIYOS_ERR_INTERNAL);
    EXPECT_EQ(seen, "internal error: now");
}

TEST(FirewallTest, NestedFailure)
{
    biyos::Env env;
    int calls = 0;
    const auto nest = [&](const biyos::Event &) { fail_inside(env, calls); };
    env.events().add_hook(EventKind::Message, EventLevel::Error, nest);
    EXPECT_EQ(status_after(env, [] { throw std::runtime_error("outer"); }), BIYOS_ERR_INTERNAL);
    EXPECT_EQ(calls, 1);
    EXPECT_STREQ(env.last_error().text, "internal error: outer");
}

TEST(FirewallTest, QuietSuccess)
{
    biyos::Env env;
    env.record_error(biyos::ErrorKind::InvalidArgument, "earlier");
    int calls = 0;
    const auto nest = [&](const biyos::Event &) { fail_inside(env, calls); };
    env.events().add_hook(EventKind::Message, EventLevel::Info, nest);
    EXPECT_EQ(status_after(env, [&] { say(env, "info"); }), BIYOS_OK);
    EXPECT_EQ(calls, 1);
    EXPECT_STREQ(env.last_error().text, "earlier");
}
