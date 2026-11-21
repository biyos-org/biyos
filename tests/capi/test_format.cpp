#include "biyos/biyos.h"
#include "capi/event.hpp"
#include "core/events.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using Format = BIYOS_status (*)(const BIYOS_event *, size_t, char *, size_t *);

constexpr std::string_view kText = "0.000000 warning presolve: removed rows rows=3";
constexpr std::string_view kJson =
    R"({"time":0.000000,"kind":"message","level":"warning",)"
    R"("module":"presolve","text":"removed rows","fields":{"rows":"3"}})";

biyos::Event removed()
{
    biyos::Event evt{};
    evt.kind = biyos::EventKind::Message;
    evt.level = biyos::EventLevel::Warning;
    evt.module = "presolve";
    evt.text = "removed rows";
    evt.fields.push_back(biyos::Field{.name = "rows", .value = "3"});
    return evt;
}

std::string fill(Format fmt, const biyos::Event &evt, std::size_t size, std::size_t &needed)
{
    const BIYOS_event view{&evt};
    std::vector<char> buffer(size, 'x');
    EXPECT_EQ(fmt(&view, buffer.size(), buffer.data(), &needed), BIYOS_OK);
    EXPECT_EQ(buffer.back(), '\0');
    return buffer.data();
}

void expect_resize(Format fmt, std::string_view line)
{
    const biyos::Event evt = removed();
    std::size_t needed = 0;
    EXPECT_EQ(fill(fmt, evt, 8, needed), line.substr(0, 7));
    const std::size_t length = needed;
    EXPECT_EQ(length, line.size());
    EXPECT_EQ(fill(fmt, evt, length, needed), line.substr(0, length - 1));
    EXPECT_EQ(fill(fmt, evt, length + 1, needed), line);
    EXPECT_EQ(needed, length);
}

std::string line_of(Format fmt, const BIYOS_event *evt)
{
    std::size_t needed = 0;
    if (fmt(evt, 0, nullptr, &needed) != BIYOS_OK)
    {
        return "failed";
    }
    std::vector<char> buffer(needed + 1);
    if (fmt(evt, buffer.size(), buffer.data(), &needed) != BIYOS_OK)
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

void keep(const BIYOS_event *evt, void *data)
{
    Lines *lines = static_cast<Lines *>(data);
    lines->text = line_of(BIYOS_fmttext, evt);
    lines->json = line_of(BIYOS_fmtjson, evt);
}

}

TEST(CApiTest, TextBuffer)
{
    expect_resize(BIYOS_fmttext, kText);
}

TEST(CApiTest, JsonBuffer)
{
    expect_resize(BIYOS_fmtjson, kJson);
}

TEST(CApiTest, OneByte)
{
    const biyos::Event evt = removed();
    std::size_t needed = 0;
    EXPECT_EQ(fill(BIYOS_fmttext, evt, 1, needed), "");
    EXPECT_EQ(needed, kText.size());
    EXPECT_EQ(fill(BIYOS_fmtjson, evt, 1, needed), "");
    EXPECT_EQ(needed, kJson.size());
}

TEST(CApiTest, SplitCharacter)
{
    biyos::Event evt = removed();
    evt.text = "caf\xc3\xa9";
    evt.fields.clear();
    const std::string line = "0.000000 warning presolve: caf\xc3\xa9";
    std::size_t needed = 0;
    EXPECT_EQ(fill(BIYOS_fmttext, evt, line.size(), needed), "0.000000 warning presolve: caf\xc3");
    EXPECT_EQ(needed, line.size());
}

TEST(CApiTest, ZeroSize)
{
    const biyos::Event evt = removed();
    const BIYOS_event view{&evt};
    std::size_t needed = 0;
    EXPECT_EQ(BIYOS_fmttext(&view, 0, nullptr, &needed), BIYOS_OK);
    EXPECT_EQ(needed, kText.size());
    char kept = 'x';
    EXPECT_EQ(BIYOS_fmtjson(&view, 0, &kept, &needed), BIYOS_OK);
    EXPECT_EQ(needed, kJson.size());
    EXPECT_EQ(kept, 'x');
}

TEST(CApiTest, BadFormat)
{
    const biyos::Event evt = removed();
    const BIYOS_event view{&evt};
    std::array<char, 4> buffer{'a', 'b', 'c', '\0'};
    std::size_t needed = 7;
    for (const Format fmt : {BIYOS_fmttext, BIYOS_fmtjson})
    {
        EXPECT_EQ(fmt(nullptr, buffer.size(), buffer.data(), &needed), BIYOS_ERR_BADARG);
        EXPECT_EQ(fmt(&view, buffer.size(), buffer.data(), nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(fmt(&view, 1, nullptr, &needed), BIYOS_ERR_BADARG);
    }
    EXPECT_STREQ(buffer.data(), "abc");
    EXPECT_EQ(needed, 7U);
}

TEST(CApiTest, ErrorLine)
{
    BIYOS_env *env = nullptr;
    ASSERT_EQ(BIYOS_newenv(&env), BIYOS_OK);
    Lines lines;
    BIYOS_index id = 0;
    const int knd = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addhook(env, keep, &lines, knd, BIYOS_LVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_setintopt(env, BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const std::string text = "invalid argument: option 'seed': -1 is outside [0, 2147483647]";
    const std::string head = R"(,"kind":"message","level":"error","module":"capi","text":")";
    const std::string tail = R"(","fields":{"code":"-100"}})";
    const std::regex stamped_text(R"((\d+\.\d{6}) (.*))");
    const std::regex stamped_json(R"(\{"time":(\d+\.\d{6})(.*))");
    std::smatch plain;
    std::smatch json;
    ASSERT_TRUE(std::regex_match(lines.text, plain, stamped_text));
    ASSERT_TRUE(std::regex_match(lines.json, json, stamped_json));
    EXPECT_EQ(plain[1], json[1]);
    EXPECT_EQ(plain[2], "error capi: " + text + " code=-100");
    EXPECT_EQ(json[2], head + text + tail);
    EXPECT_EQ(BIYOS_freeenv(env), BIYOS_OK);
}
