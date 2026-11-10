#include "capi/firewall.hpp"

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
