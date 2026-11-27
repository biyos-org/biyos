#pragma once

#include "core/error.hpp"
#include "io/native/native.hpp"
#include "io/native/print.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <string_view>

namespace biyos::test
{

inline std::string native_text(const Model &model)
{
    std::ostringstream output;
    print_native(model, output);
    return output.str();
}

inline Model native_model(std::string_view text)
{
    std::istringstream input{std::string(text)};
    return read_native(input, "t.bys");
}

inline std::string native_fault(std::string_view text)
{
    try
    {
        native_model(text);
    }
    catch (const Error &error)
    {
        return error.what();
    }
    return "";
}

inline std::string bad_line(int line, std::string_view message)
{
    return "bad file: file 't.bys' line " + std::to_string(line) + ": " + std::string(message);
}

inline void expect_round(const Model &model)
{
    const std::string text = native_text(model);
    const Model back = native_model(text);
    EXPECT_EQ(difference(model, back), "");
    EXPECT_EQ(native_text(back), text);
}

}
