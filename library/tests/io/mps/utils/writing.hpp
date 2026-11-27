#pragma once

#include "core/env/error.hpp"
#include "io/draft/sketch.hpp"
#include "io/mps/print.hpp"
#include "model/model.hpp"

#include <sstream>
#include <string>

namespace biyos::test
{

inline std::string printed(const Model &model)
{
    std::ostringstream output;
    print_mps(draft_of(model), output);
    return output.str();
}

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
