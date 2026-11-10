#include "core/events.hpp"
#include "core/format.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

namespace
{

using biyos::Level;
using namespace std::chrono_literals;

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

biyos::Event stamped(std::chrono::nanoseconds time)
{
    biyos::Event event = message(Level::Info, "a");
    event.time = time;
    return event;
}

std::string text_at(std::chrono::nanoseconds time)
{
    return biyos::format_text(stamped(time));
}

std::string json_at(std::chrono::nanoseconds time)
{
    return biyos::format_json(stamped(time));
}

std::string json_of(std::string text)
{
    return biyos::format_json(message(Level::Info, std::move(text)));
}

std::string json_line(std::string_view quoted)
{
    const std::string head =
        R"({"time":0.000000,"kind":"message","level":"info","module":"core","text":)";
    return head + std::string(quoted) + R"(,"fields":{}})";
}

}

TEST(FormatTest, Text)
{
    EXPECT_EQ(biyos::format_text(removed()), "0.000000 info core: removed rows rows=3 columns=1");
}

TEST(FormatTest, TextLevels)
{
    EXPECT_EQ(biyos::format_text(message(Level::Debug, "a")), "0.000000 debug core: a");
    EXPECT_EQ(biyos::format_text(message(Level::Info, "a")), "0.000000 info core: a");
    EXPECT_EQ(biyos::format_text(message(Level::Warning, "a")), "0.000000 warning core: a");
    EXPECT_EQ(biyos::format_text(message(Level::Error, "a")), "0.000000 error core: a");
}

TEST(FormatTest, EmptyText)
{
    biyos::Event event = removed();
    event.text.clear();
    EXPECT_EQ(biyos::format_text(event), "0.000000 info core: rows=3 columns=1");
    event.fields.clear();
    EXPECT_EQ(biyos::format_text(event), "0.000000 info core:");
}

TEST(FormatTest, TextEscapes)
{
    EXPECT_EQ(text_of("a\nb"), "0.000000 info core: a\\nb");
    EXPECT_EQ(text_of("a\tb"), "0.000000 info core: a\\tb");
    EXPECT_EQ(text_of("a\rb"), "0.000000 info core: a\\u000db");
    EXPECT_EQ(text_of(std::string("a\0b", 3)), "0.000000 info core: a\\u0000b");
    EXPECT_EQ(text_of("\x01\x1f"), "0.000000 info core: \\u0001\\u001f");
    EXPECT_EQ(text_of("\x7f \"q\" \\ caf\xc3\xa9"),
              "0.000000 info core: \x7f \"q\" \\ caf\xc3\xa9");
}

TEST(FormatTest, TextFields)
{
    biyos::Event event = message(Level::Info, "");
    event.module = "a\nb";
    event.fields.push_back(biyos::Field{.name = "x\ty", .value = "1\n2"});
    event.fields.push_back(biyos::Field{.name = "", .value = ""});
    EXPECT_EQ(biyos::format_text(event), "0.000000 info a\\nb: x\\ty=1\\n2 =");
}

TEST(FormatTest, TextTime)
{
    EXPECT_EQ(text_at(0ns), "0.000000 info core: a");
    EXPECT_EQ(text_at(999ns), "0.000000 info core: a");
    EXPECT_EQ(text_at(1us), "0.000001 info core: a");
    EXPECT_EQ(text_at(1234567890ns), "1.234567 info core: a");
    EXPECT_EQ(text_at(3600s), "3600.000000 info core: a");
    EXPECT_EQ(text_at(-1500ns), "-0.000001 info core: a");
    EXPECT_EQ(text_at(-2s), "-2.000000 info core: a");
    const auto largest = (std::chrono::nanoseconds::max)();
    EXPECT_EQ(text_at(largest), "9223372036.854775 info core: a");
}

TEST(FormatTest, JsonTime)
{
    const std::string tail = R"(,"kind":"message","level":"info","module":"core","text":"a",)"
                             R"("fields":{}})";
    const std::string later = R"({"time":1.234567)";
    const std::string earlier = R"({"time":-0.000001)";
    EXPECT_EQ(json_at(1234567890ns), later + tail);
    EXPECT_EQ(json_at(-1500ns), earlier + tail);
}

TEST(FormatTest, Json)
{
    const std::string line = R"({"time":0.000000,"kind":"message","level":"info","module":"core",)"
                             R"("text":"removed rows","fields":{"rows":"3","columns":"1"}})";
    EXPECT_EQ(biyos::format_json(removed()), line);
}

TEST(FormatTest, JsonLevels)
{
    const std::string tail = R"(","module":"core","text":"","fields":{}})";
    const std::string head = R"({"time":0.000000,"kind":"message","level":")";
    EXPECT_EQ(biyos::format_json(message(Level::Debug, "")), head + "debug" + tail);
    EXPECT_EQ(biyos::format_json(message(Level::Info, "")), head + "info" + tail);
    EXPECT_EQ(biyos::format_json(message(Level::Warning, "")), head + "warning" + tail);
    EXPECT_EQ(biyos::format_json(message(Level::Error, "")), head + "error" + tail);
}

TEST(FormatTest, JsonEscapes)
{
    const std::string quote = R"("say \"hi\"")";
    const std::string backslash = R"("a\\b")";
    const std::string newline = R"("a\nb")";
    const std::string tab = R"("a\tb")";
    const std::string others = R"("a\u000db\u0008c\u000cd")";
    const std::string zero = R"("a\u0000b")";
    const std::string bounds = R"("\u0001\u001f")";
    EXPECT_EQ(json_of("say \"hi\""), json_line(quote));
    EXPECT_EQ(json_of("a\\b"), json_line(backslash));
    EXPECT_EQ(json_of("a\nb"), json_line(newline));
    EXPECT_EQ(json_of("a\tb"), json_line(tab));
    EXPECT_EQ(json_of("a\rb\bc\fd"), json_line(others));
    EXPECT_EQ(json_of(std::string("a\0b", 3)), json_line(zero));
    EXPECT_EQ(json_of("\x01\x1f"), json_line(bounds));
    EXPECT_EQ(json_of("\x7f /"), json_line("\"\x7f /\""));
    const std::string utf8 = "caf\xc3\xa9 \xe2\x82\xac \xf0\x9d\x84\x9e";
    EXPECT_EQ(json_of(utf8), json_line("\"" + utf8 + "\""));
}

TEST(FormatTest, Malformed)
{
    const std::string mark = "\xef\xbf\xbd";
    EXPECT_EQ(json_of("seed\xff"), json_line("\"seed" + mark + "\""));
    EXPECT_EQ(json_of("\xe2\x82 \x80"), json_line("\"" + mark + " " + mark + "\""));
    EXPECT_EQ(json_of("\xed\xa0\x80"), json_line("\"" + mark + mark + mark + "\""));
    EXPECT_EQ(json_of("\xc3\xa9\xc3"), json_line("\"\xc3\xa9" + mark + "\""));
    EXPECT_EQ(text_of("seed\xff"), "0.000000 info core: seed\xff");
}

TEST(FormatTest, JsonFields)
{
    biyos::Event event = message(Level::Info, "");
    event.module = "a\"b";
    event.fields.push_back(biyos::Field{.name = "x\ny", .value = "\\"});
    event.fields.push_back(biyos::Field{.name = "", .value = ""});
    const std::string line =
        R"({"time":0.000000,"kind":"message","level":"info","module":"a\"b","text":"",)"
        R"("fields":{"x\ny":"\\","":""}})";
    EXPECT_EQ(biyos::format_json(event), line);
}
