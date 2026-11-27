#include "core/env/error.hpp"

#include <gtest/gtest.h>

#include <exception>
#include <type_traits>

using biyos::Error;
using biyos::ErrorKind;

static_assert(std::is_nothrow_copy_constructible_v<Error>);

TEST(ErrorTest, InvalidArgument)
{
    const Error error(ErrorKind::InvalidArgument, "argument 'n'", "must be positive");
    EXPECT_EQ(error.kind(), ErrorKind::InvalidArgument);
    EXPECT_EQ(error.object(), "argument 'n'");
    EXPECT_EQ(error.message(), "must be positive");
    EXPECT_STREQ(error.what(), "invalid argument: argument 'n': must be positive");
}

TEST(ErrorTest, BadFile)
{
    const Error error(ErrorKind::BadFile, "file 'a.mps' line 3", "unknown row 'r'");
    EXPECT_STREQ(error.what(), "bad file: file 'a.mps' line 3: unknown row 'r'");
}

TEST(ErrorTest, Unsupported)
{
    const Error error(ErrorKind::Unsupported, "file 'a.mps' line 9", "section QUADOBJ");
    EXPECT_STREQ(error.what(), "unsupported: file 'a.mps' line 9: section QUADOBJ");
}

TEST(ErrorTest, Busy)
{
    const Error error(ErrorKind::Busy, "model", "is in a call on another thread");
    EXPECT_EQ(error.kind(), ErrorKind::Busy);
    EXPECT_STREQ(error.what(), "busy: model: is in a call on another thread");
}

TEST(ErrorTest, InUse)
{
    const Error error(ErrorKind::InUse, "variable 2", "indicator 0 needs it binary");
    EXPECT_EQ(error.kind(), ErrorKind::InUse);
    EXPECT_STREQ(error.what(), "in use: variable 2: indicator 0 needs it binary");
}

TEST(ErrorTest, Callback)
{
    const Error error(ErrorKind::Callback, "callback 1", "returned -2");
    EXPECT_EQ(error.kind(), ErrorKind::Callback);
    EXPECT_STREQ(error.what(), "callback: callback 1: returned -2");
}

TEST(ErrorTest, NoMemory)
{
    const Error error(ErrorKind::NoMemory, "workspace", "needs 3 GB");
    EXPECT_EQ(error.kind(), ErrorKind::NoMemory);
    EXPECT_STREQ(error.what(), "out of memory: workspace: needs 3 GB");
}

TEST(ErrorTest, FileAccess)
{
    const Error error(ErrorKind::FileAccess, "file 'a.mps'", "cannot be opened");
    EXPECT_STREQ(error.what(), "file access: file 'a.mps': cannot be opened");
}

TEST(ErrorTest, Internal)
{
    const Error error(ErrorKind::Internal, "factorization", "pivot is not finite");
    EXPECT_EQ(error.kind(), ErrorKind::Internal);
    EXPECT_EQ(error.object(), "factorization");
    EXPECT_EQ(error.message(), "pivot is not finite");
    EXPECT_STREQ(error.what(), "internal error: factorization: pivot is not finite");
}

TEST(ErrorTest, EmptyObject)
{
    const Error error(ErrorKind::Internal, "", "unreachable state");
    EXPECT_STREQ(error.what(), "internal error: unreachable state");
}

TEST(ErrorTest, SharedText)
{
    const Error error(ErrorKind::InvalidArgument, "option 'x'", "unknown");
    const Error copy = error;
    EXPECT_EQ(static_cast<const void *>(copy.what()), static_cast<const void *>(error.what()));
}

TEST(ErrorTest, StdException)
{
    try
    {
        throw Error(ErrorKind::InvalidArgument, "argument 'm'", "is null");
    }
    catch (const std::exception &caught)
    {
        EXPECT_STREQ(caught.what(), "invalid argument: argument 'm': is null");
        return;
    }
    FAIL() << "the error was not caught as std::exception";
}
