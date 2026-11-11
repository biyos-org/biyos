#include "model/model.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace
{

using biyos::Model;
using biyos::test::refusal;

std::string held(const std::string *name)
{
    return name == nullptr ? "none" : "'" + *name + "'";
}

std::string model_refusal(Model &model, std::string_view name)
{
    return refusal([&] { model.set_name(name); });
}

}

TEST(NameTest, ModelName)
{
    Model model;
    EXPECT_EQ(model.name(), nullptr);
    model.set_name("plant");
    EXPECT_EQ(held(model.name()), "'plant'");
    model.set_name("\xc3\xa9t\xc3\xa9 \xf0\x9f\x98\x80");
    EXPECT_EQ(held(model.name()), "'\xc3\xa9t\xc3\xa9 \xf0\x9f\x98\x80'");
    model.del_name();
    EXPECT_EQ(model.name(), nullptr);
    model.del_name();
    EXPECT_EQ(model.name(), nullptr);
    model.set_name("plant");
    EXPECT_EQ(held(model.name()), "'plant'");
}

TEST(NameTest, Text)
{
    Model model;
    model.set_name("kept");
    const std::string prefix = "invalid argument: model: name ";
    EXPECT_EQ(model_refusal(model, ""), prefix + "is empty");
    EXPECT_EQ(model_refusal(model, std::string_view("\0", 1)), prefix + "holds a NUL at byte 0");
    EXPECT_EQ(model_refusal(model, std::string_view("a\xc3\xa9\0", 4)),
              prefix + "holds a NUL at byte 3");
    EXPECT_EQ(model_refusal(model, "ab\xff"), prefix + "is not valid UTF-8 at byte 2");
    EXPECT_EQ(model_refusal(model, "\xc3"), prefix + "is not valid UTF-8 at byte 0");
    EXPECT_EQ(model_refusal(model, "x\xc0\xaf"), prefix + "is not valid UTF-8 at byte 1");
    EXPECT_EQ(model_refusal(model, "\xe2\x82\xac\xed\xa0\x80"),
              prefix + "is not valid UTF-8 at byte 3");
    EXPECT_EQ(model_refusal(model, "\xf4\x90\x80\x80"), prefix + "is not valid UTF-8 at byte 0");
    EXPECT_EQ(model_refusal(model, "\xe2\x82 "), prefix + "is not valid UTF-8 at byte 0");
    EXPECT_EQ(held(model.name()), "'kept'");
}
