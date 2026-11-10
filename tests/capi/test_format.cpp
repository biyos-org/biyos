#include "biyos/biyos.h"
#include "capi/event.hpp"
#include "core/events.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using Format = biyos_status (*)(const biyos_event *, char *, size_t, size_t *);

constexpr std::string_view kText = "warning presolve: removed rows rows=3";
constexpr std::string_view kJson = R"({"kind":"message","level":"warning","module":"presolve",)"
                                   R"("text":"removed rows","fields":{"rows":"3"}})";

biyos::Event removed()
{
    biyos::Event event{};
    event.kind = biyos::EventKind::Message;
    event.level = biyos::Level::Warning;
    event.module = "presolve";
    event.text = "removed rows";
    event.fields.push_back(biyos::Field{.name = "rows", .value = "3"});
    return event;
}

std::string fill(Format format, const biyos::Event &event, std::size_t size, std::size_t &needed)
{
    const biyos_event view{&event};
    std::vector<char> buffer(size, 'x');
    EXPECT_EQ(format(&view, buffer.data(), buffer.size(), &needed), BIYOS_OK);
    EXPECT_EQ(buffer.back(), '\0');
    return buffer.data();
}

void expect_resize(Format format, std::string_view line)
{
    const biyos::Event event = removed();
    std::size_t needed = 0;
    EXPECT_EQ(fill(format, event, 8, needed), line.substr(0, 7));
    const std::size_t length = needed;
    EXPECT_EQ(length, line.size());
    EXPECT_EQ(fill(format, event, length, needed), line.substr(0, length - 1));
    EXPECT_EQ(fill(format, event, length + 1, needed), line);
    EXPECT_EQ(needed, length);
}

std::string line_of(Format format, const biyos_event *event)
{
    std::size_t needed = 0;
    if (format(event, nullptr, 0, &needed) != BIYOS_OK)
    {
        return "failed";
    }
    std::vector<char> buffer(needed + 1);
    if (format(event, buffer.data(), buffer.size(), &needed) != BIYOS_OK)
    {
        return "failed";
    }
    return buffer.data();
}

struct Lines
{
    std::string text;
    std::string json;
};

void keep(const biyos_event *event, void *user_data)
{
    Lines *lines = static_cast<Lines *>(user_data);
    lines->text = line_of(biyos_format_text, event);
    lines->json = line_of(biyos_format_json, event);
}

}

TEST(CApiTest, TextBuffer)
{
    expect_resize(biyos_format_text, kText);
}

TEST(CApiTest, JsonBuffer)
{
    expect_resize(biyos_format_json, kJson);
}

TEST(CApiTest, OneByte)
{
    const biyos::Event event = removed();
    std::size_t needed = 0;
    EXPECT_EQ(fill(biyos_format_text, event, 1, needed), "");
    EXPECT_EQ(needed, kText.size());
    EXPECT_EQ(fill(biyos_format_json, event, 1, needed), "");
    EXPECT_EQ(needed, kJson.size());
}

TEST(CApiTest, SplitCharacter)
{
    biyos::Event event = removed();
    event.text = "caf\xc3\xa9";
    event.fields.clear();
    const std::string line = "warning presolve: caf\xc3\xa9";
    std::size_t needed = 0;
    EXPECT_EQ(fill(biyos_format_text, event, line.size(), needed), "warning presolve: caf\xc3");
    EXPECT_EQ(needed, line.size());
}

TEST(CApiTest, ZeroSize)
{
    const biyos::Event event = removed();
    const biyos_event view{&event};
    std::size_t needed = 0;
    EXPECT_EQ(biyos_format_text(&view, nullptr, 0, &needed), BIYOS_OK);
    EXPECT_EQ(needed, kText.size());
    char kept = 'x';
    EXPECT_EQ(biyos_format_json(&view, &kept, 0, &needed), BIYOS_OK);
    EXPECT_EQ(needed, kJson.size());
    EXPECT_EQ(kept, 'x');
}

TEST(CApiTest, BadFormat)
{
    const biyos::Event event = removed();
    const biyos_event view{&event};
    std::array<char, 4> buffer{'a', 'b', 'c', '\0'};
    std::size_t needed = 7;
    for (const Format format : {biyos_format_text, biyos_format_json})
    {
        EXPECT_EQ(format(nullptr, buffer.data(), buffer.size(), &needed), BIYOS_INVALID_ARGUMENT);
        EXPECT_EQ(format(&view, buffer.data(), buffer.size(), nullptr), BIYOS_INVALID_ARGUMENT);
        EXPECT_EQ(format(&view, nullptr, 1, &needed), BIYOS_INVALID_ARGUMENT);
    }
    EXPECT_STREQ(buffer.data(), "abc");
    EXPECT_EQ(needed, 7U);
}

TEST(CApiTest, ErrorLine)
{
    biyos_env *env = nullptr;
    ASSERT_EQ(biyos_env_create(&env), BIYOS_OK);
    Lines lines;
    int id = 0;
    const int kind = BIYOS_EVENT_MESSAGE;
    ASSERT_EQ(biyos_subscribe(env, keep, &lines, kind, BIYOS_LEVEL_ERROR, &id), BIYOS_OK);
    EXPECT_EQ(biyos_set_int_option(env, BIYOS_OPT_SEED, -1), BIYOS_INVALID_ARGUMENT);
    const std::string text = "invalid argument: option 'seed': -1 is outside [0, 2147483647]";
    const std::string head = R"({"kind":"message","level":"error","module":"capi","text":")";
    const std::string tail = R"(","fields":{"code":"-100"}})";
    EXPECT_EQ(lines.text, "error capi: " + text + " code=-100");
    EXPECT_EQ(lines.json, head + text + tail);
    EXPECT_EQ(biyos_env_free(env), BIYOS_OK);
}
