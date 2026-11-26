#include "core/text/name.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <type_traits>

using biyos::StaticName;

namespace
{

constexpr char kPadded[8] = "nlp";

static_assert(!std::is_constructible_v<StaticName, const char *>);
static_assert(!std::is_constructible_v<StaticName, std::string>);
static_assert(!std::is_constructible_v<StaticName, std::string_view>);

}

TEST(NameTest, Literal)
{
    constexpr StaticName name("presolve");
    static_assert(name.text() == "presolve");
    EXPECT_EQ(name.text(), "presolve");
}

TEST(NameTest, ConstantArray)
{
    constexpr StaticName name(kPadded);
    EXPECT_EQ(name.text(), "nlp");
}
