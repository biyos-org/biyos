#include "core/name.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <type_traits>

namespace
{

constexpr char kPadded[8] = "nlp";

static_assert(!std::is_constructible_v<biyos::FixedName, const char *>);
static_assert(!std::is_constructible_v<biyos::FixedName, std::string>);
static_assert(!std::is_constructible_v<biyos::FixedName, std::string_view>);

}

TEST(NameTest, Literal)
{
    constexpr biyos::FixedName name("presolve");
    static_assert(name.text() == "presolve");
    EXPECT_EQ(name.text(), "presolve");
}

TEST(NameTest, ConstantArray)
{
    constexpr biyos::FixedName name(kPadded);
    EXPECT_EQ(name.text(), "nlp");
}
