#include "capi/firewall.hpp"
#include "core/environment.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <new>
#include <stdexcept>

namespace
{

biyos_status status_after(const std::function<void()> &body)
{
    return biyos::capi::guard(body);
}

biyos_status status_after(biyos::Environment &env, const std::function<void()> &body)
{
    return biyos::capi::guard(&env, body);
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
    EXPECT_EQ(env.last_error(), "foreign");
}

TEST(FirewallTest, UnknownMessage)
{
    biyos::Environment env;
    EXPECT_EQ(status_after(env, [] { throw 42; }), BIYOS_INTERNAL_ERROR);
    EXPECT_EQ(env.last_error(), "unknown exception");
}

TEST(FirewallTest, KeptMessage)
{
    biyos::Environment env;
    env.record_error("earlier");
    EXPECT_EQ(status_after(env, [] {}), BIYOS_OK);
    EXPECT_EQ(env.last_error(), "earlier");
}
