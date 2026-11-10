#include "core/events.hpp"
#include "core/format.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace
{

using biyos::Level;

biyos::Event message(Level level, std::string text)
{
    biyos::Event event{};
    event.kind = biyos::EventKind::Message;
    event.level = level;
    event.module = "core";
    event.text = std::move(text);
    return event;
}

biyos::Event removed()
{
    biyos::Event event = message(Level::Info, "removed rows");
    event.fields.push_back(biyos::Field{.name = "rows", .value = "3"});
    event.fields.push_back(biyos::Field{.name = "columns", .value = "1"});
    return event;
}

std::string text_of(std::string text)
{
    return biyos::format_text(message(Level::Info, std::move(text)));
}

}

TEST(FormatTest, Text)
{
    EXPECT_EQ(biyos::format_text(removed()), "info core: removed rows rows=3 columns=1");
}

TEST(FormatTest, TextLevels)
{
    EXPECT_EQ(biyos::format_text(message(Level::Debug, "a")), "debug core: a");
    EXPECT_EQ(biyos::format_text(message(Level::Info, "a")), "info core: a");
    EXPECT_EQ(biyos::format_text(message(Level::Warning, "a")), "warning core: a");
    EXPECT_EQ(biyos::format_text(message(Level::Error, "a")), "error core: a");
}

TEST(FormatTest, EmptyText)
{
    biyos::Event event = removed();
    event.text.clear();
    EXPECT_EQ(biyos::format_text(event), "info core: rows=3 columns=1");
    event.fields.clear();
    EXPECT_EQ(biyos::format_text(event), "info core:");
}

TEST(FormatTest, TextEscapes)
{
    EXPECT_EQ(text_of("a\nb"), "info core: a\\nb");
    EXPECT_EQ(text_of("a\tb"), "info core: a\\tb");
    EXPECT_EQ(text_of("a\rb"), "info core: a\\u000db");
    EXPECT_EQ(text_of(std::string("a\0b", 3)), "info core: a\\u0000b");
    EXPECT_EQ(text_of("\x01\x1f"), "info core: \\u0001\\u001f");
    EXPECT_EQ(text_of("\x7f \"q\" \\ caf\xc3\xa9"), "info core: \x7f \"q\" \\ caf\xc3\xa9");
}

TEST(FormatTest, TextFields)
{
    biyos::Event event = message(Level::Info, "");
    event.module = "a\nb";
    event.fields.push_back(biyos::Field{.name = "x\ty", .value = "1\n2"});
    event.fields.push_back(biyos::Field{.name = "", .value = ""});
    EXPECT_EQ(biyos::format_text(event), "info a\\nb: x\\ty=1\\n2 =");
}
