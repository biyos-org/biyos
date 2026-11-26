#pragma once

#include "core/error.hpp"

#include <gtest/gtest.h>

#include <string>

namespace biyos::test
{

template <typename Call> std::string failure(Call call, biyos::ErrorKind kind)
{
    try
    {
        call();
    }
    catch (const biyos::Error &error)
    {
        EXPECT_EQ(error.kind(), kind);
        return error.what();
    }
    ADD_FAILURE() << "no error";
    return "";
}

template <typename Call> std::string refusal(Call call)
{
    return failure(call, biyos::ErrorKind::InvalidArgument);
}

}
