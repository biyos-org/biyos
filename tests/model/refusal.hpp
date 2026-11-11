#pragma once

#include "core/error.hpp"

#include <gtest/gtest.h>

#include <string>

namespace biyos::test
{

template <typename Call> std::string refusal(Call call)
{
    try
    {
        call();
    }
    catch (const biyos::Error &error)
    {
        EXPECT_EQ(error.kind(), biyos::ErrorKind::InvalidArgument);
        return error.what();
    }
    ADD_FAILURE() << "no error";
    return "";
}

}
