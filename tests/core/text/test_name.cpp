#include "core/text/name.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <type_traits>

namespace
{

constexpr char kPadded[8] = "nlp";

static_assert(!std::is_constructible_v<biyos::StaticName, const char *>);
static_assert(!std::is_constructible_v<biyos::StaticName, std::string>);
static_assert(!std::is_constructible_v<biyos::StaticName, std::string_view>);

}

TEST(NameTest, Literal)
{
    constexpr biyos::StaticName name("presolve");
    static_assert(name.text() == "presolve");
    EXPECT_EQ(name.text(), "presolve");
}

TEST(NameTest, ConstantArray)
{
    constexpr biyos::StaticName name(kPadded);
    EXPECT_EQ(name.text(), "nlp");
}
