#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/events.hpp"
#include "core/option_table.hpp"
#include "core/options.hpp"
#include "core/settings.hpp"
#include "core/version.hpp"

#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace
{

using biyos::EventKind;
using biyos::EventLevel;
using biyos::IntRule;
using biyos::Option;
using biyos::OptionSpec;
using biyos::OptionValue;
using biyos::Scope;
using biyos::Version;

constexpr Version kFirst{0, 0, 1};
constexpr Version kSecond{0, 0, 2};
constexpr Version kThird{0, 0, 3};
constexpr Option kNew{1};
constexpr Option kOld{2};
constexpr Option kGone{3};
constexpr Option kHost{4};

constexpr std::array kRows{
    OptionSpec{
        .id = kNew,
        .name = "new",
        .scope = Scope::Model,
        .rule = IntRule{.default_value = 1, .lower = 0},
        .doc = "Current.",
        .versions = {.since = kFirst},
    },
    OptionSpec{
        .id = kOld,
        .name = "old",
        .scope = Scope::Model,
        .rule = IntRule{.default_value = 1, .lower = 0},
        .doc = "Deprecated.",
        .versions = {.since = kFirst, .deprecated = kSecond},
        .replacement = kNew,
    },
    OptionSpec{
        .id = kGone,
        .name = "gone",
        .scope = Scope::Model,
        .rule = IntRule{.default_value = 1, .lower = 0},
        .doc = "Retired.",
        .versions = {.since = kFirst, .deprecated = kSecond, .retired = kThird},
        .replacement = kNew,
    },
    OptionSpec{
        .id = kHost,
        .name = "host",
        .scope = Scope::Environment,
        .rule = IntRule{.default_value = 1, .lower = 0},
        .doc = "Environment only.",
        .versions = {.since = kFirst},
    },
};

std::string failure(const std::function<void()> &body)
{
    try
    {
        body();
    }
    catch (const biyos::Error &error)
    {
        return error.what();
    }
    return "no error";
}

int held(const biyos::Settings &settings, Option option)
{
    const OptionValue *value = settings.find(option);
    return value != nullptr ? std::get<int>(*value) : -1;
}

class SettingTest : public ::testing::Test
{
  protected:
    SettingTest()
    {
        const auto record = [this](const biyos::Event &event) { heard.push_back(event); };
        events.add_hook(EventKind::Message, EventLevel::Debug, record);
    }

    biyos::FakeClock clock;
    biyos::Events events{clock};
    std::vector<biyos::Event> heard;
    biyos::Options options{kRows, events};
    biyos::Settings settings{kRows};
};

}

TEST_F(SettingTest, Empty)
{
    const biyos::Settings none;
    for (const OptionSpec &spec : biyos::option_table())
    {
        EXPECT_EQ(none.find(spec.id), nullptr) << spec.name;
    }
    for (const OptionSpec &spec : kRows)
    {
        EXPECT_EQ(settings.find(spec.id), nullptr) << spec.name;
    }
}

TEST_F(SettingTest, Set)
{
    settings.set(kNew, 5, events);
    EXPECT_EQ(held(settings, kNew), 5);
    settings.set(kNew, 7, events);
    EXPECT_EQ(held(settings, kNew), 7);
    EXPECT_EQ(settings.find(kOld), nullptr);
    EXPECT_EQ(options.get_int(kNew), 1);
    EXPECT_TRUE(heard.empty());
}

TEST_F(SettingTest, Erase)
{
    settings.set(kNew, 5, events);
    settings.set(kOld, 6, events);
    settings.erase(kNew);
    EXPECT_EQ(settings.find(kNew), nullptr);
    EXPECT_EQ(held(settings, kOld), 6);
    settings.erase(kNew);
    settings.erase(kGone);
    EXPECT_EQ(held(settings, kOld), 6);
}

TEST_F(SettingTest, Copy)
{
    settings.set(kNew, 5, events);
    biyos::Settings copy = settings;
    copy.set(kNew, 8, events);
    copy.erase(kOld);
    EXPECT_EQ(held(settings, kNew), 5);
    EXPECT_EQ(held(copy, kNew), 8);
    copy = settings;
    EXPECT_EQ(held(copy, kNew), 5);
}

TEST_F(SettingTest, Scope)
{
    const std::string scope = "invalid argument: option 'host': has environment scope";
    EXPECT_EQ(failure([&] { settings.set(kHost, 3, events); }), scope);
    EXPECT_EQ(failure([&] { settings.set(kHost, -1, events); }), scope);
    EXPECT_EQ(failure([&] { settings.set(kHost, 2.0, events); }), scope);
    EXPECT_EQ(settings.find(kHost), nullptr);
    options.set_int(kHost, 3);
    EXPECT_EQ(options.get_int(kHost), 3);
}

TEST_F(SettingTest, Refusal)
{
    settings.set(kNew, 5, events);
    const std::string range = "invalid argument: option 'new': -1 is outside [0, 2147483647]";
    EXPECT_EQ(failure([&] { settings.set(kNew, -1, events); }), range);
    EXPECT_EQ(failure([&] { options.set_int(kNew, -1); }), range);
    const std::string kind = "invalid argument: option 'new': is an int, not a double";
    EXPECT_EQ(failure([&] { settings.set(kNew, 2.0, events); }), kind);
    EXPECT_EQ(failure([&] { options.set_double(kNew, 2.0); }), kind);
    const std::string retired = "invalid argument: option 'gone': retired in 0.0.3, use 'new'";
    EXPECT_EQ(failure([&] { settings.set(kGone, 2, events); }), retired);
    EXPECT_EQ(failure([&] { options.set_int(kGone, 2); }), retired);
    EXPECT_EQ(held(settings, kNew), 5);
    EXPECT_EQ(settings.find(kGone), nullptr);
    EXPECT_TRUE(heard.empty());
}

TEST_F(SettingTest, Deprecated)
{
    settings.set(kOld, 5, events);
    options.set_int(kOld, 6);
    EXPECT_EQ(held(settings, kOld), 5);
    ASSERT_EQ(heard.size(), 2U);
    for (const biyos::Event &event : heard)
    {
        EXPECT_EQ(event.kind, EventKind::Message);
        EXPECT_EQ(event.level, EventLevel::Warning);
        EXPECT_EQ(event.module, "core");
        EXPECT_EQ(event.text, "option 'old': deprecated in 0.0.2, use 'new'");
        EXPECT_EQ(event.fields.size(), 3U);
    }
}
