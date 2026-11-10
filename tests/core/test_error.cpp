#include "core/error.hpp"

#include <gtest/gtest.h>

#include <exception>
#include <type_traits>

static_assert(std::is_nothrow_copy_constructible_v<biyos::Error>);

TEST(ErrorTest, InvalidArgument)
{
    const biyos::Error error(biyos::ErrorKind::InvalidArgument, "argument 'n'", "must be positive");
    EXPECT_EQ(error.kind(), biyos::ErrorKind::InvalidArgument);
    EXPECT_EQ(error.object(), "argument 'n'");
    EXPECT_EQ(error.message(), "must be positive");
    EXPECT_STREQ(error.what(), "invalid argument: argument 'n': must be positive");
}

TEST(ErrorTest, Internal)
{
    const biyos::Error error(biyos::ErrorKind::Internal, "factorization", "pivot is not finite");
    EXPECT_EQ(error.kind(), biyos::ErrorKind::Internal);
    EXPECT_EQ(error.object(), "factorization");
    EXPECT_EQ(error.message(), "pivot is not finite");
    EXPECT_STREQ(error.what(), "internal error: factorization: pivot is not finite");
}

TEST(ErrorTest, EmptyObject)
{
    const biyos::Error error(biyos::ErrorKind::Internal, "", "unreachable state");
    EXPECT_STREQ(error.what(), "internal error: unreachable state");
}

TEST(ErrorTest, SharedText)
{
    const biyos::Error error(biyos::ErrorKind::InvalidArgument, "option 'x'", "unknown");
    const biyos::Error copy = error;
    EXPECT_EQ(static_cast<const void *>(copy.what()), static_cast<const void *>(error.what()));
}

TEST(ErrorTest, StdException)
{
    try
    {
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, "argument 'm'", "is null");
    }
    catch (const std::exception &caught)
    {
        EXPECT_STREQ(caught.what(), "invalid argument: argument 'm': is null");
        return;
    }
    FAIL() << "the error was not caught as std::exception";
}
