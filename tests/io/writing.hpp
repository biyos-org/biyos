#pragma once

#include "core/error.hpp"
#include "io/sketch.hpp"
#include "model/model.hpp"

#include <string>

namespace biyos::test
{

inline std::string refusal(const Model &model)
{
    try
    {
        static_cast<void>(draft_of(model));
    }
    catch (const Error &error)
    {
        return error.what();
    }
    return "";
}

}
