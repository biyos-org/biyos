#include "core/env/env.hpp"
#include "core/env/error.hpp"
#include "core/env/version.hpp"
#include "core/events/events.hpp"
#include "core/events/stream.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "core/options/settings.hpp"
#include "core/options/table.hpp"

#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{

using biyos::Env;
using biyos::Error;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::IntRule;
using biyos::Lookup;
using biyos::Option;
using biyos::option_table;
using biyos::Options;
using biyos::OptionSource;
using biyos::OptionSpec;
using biyos::OptionValue;
using biyos::Scope;
using biyos::Settings;
using biyos::StringRule;
using biyos::Version;

constexpr Version kFirst{0, 0, 1};
constexpr Version kSecond{0, 0, 2};
constexpr Version kThird{0, 0, 3};
constexpr Option kNew{1};
constexpr Option kOld{2};
constexpr Option kGone{3};
constexpr Option kHost{4};
constexpr Option kText{5};

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
    OptionSpec{
        .id = kText,
        .name = "text",
        .scope = Scope::Model,
        .rule = StringRule{.default_value = ""},
        .doc = "A string.",
        .versions = {.since = kFirst},
    },
};

std::string failure(const std::function<void()> &body)
{
    try
    {
        body();
    }
    catch (const Error &error)
    {
        return error.what();
    }
    return "no error";
}

int held(const Settings &settings, Option option)
{
    const OptionValue *value = settings.find(option);
    return value != nullptr ? std::get<int>(*value) : -1;
}

std::string_view text(const Settings &settings, Option option)
{
    return std::get<std::string_view>(*settings.find(option));
}

class SettingTest : public ::testing::Test
{
  protected:
    SettingTest()
    {
        const auto record = [this](const Event &event) { heard.push_back(event); };
        env.events().add_hook(EventKind::Message, EventLevel::Debug, record);
    }

    Env env;
    std::vector<Event> heard;
    Options options{kRows, env};
    Settings settings{kRows};
};

class LookupTest : public SettingTest
{
  protected:
    int found(Option option) const { return std::get<int>(lookup.find(option).value); }

    OptionSource source(Option option) const { return lookup.find(option).source; }

    Lookup lookup{settings, options};
};

}

TEST_F(SettingTest, Empty)
{
    const Settings none;
    EXPECT_TRUE(none.empty());
    EXPECT_TRUE(settings.empty());
    for (const OptionSpec &spec : option_table())
    {
        if (spec.scope == Scope::Model)
        {
            EXPECT_EQ(none.find(spec.id), nullptr) << spec.name;
        }
    }
    for (const OptionSpec &spec : kRows)
    {
        if (spec.scope == Scope::Model)
        {
            EXPECT_EQ(settings.find(spec.id), nullptr) << spec.name;
        }
    }
}

TEST_F(SettingTest, Set)
{
    settings.set_int(kNew, 5, env.stream());
    EXPECT_EQ(held(settings, kNew), 5);
    settings.set_int(kNew, 7, env.stream());
    EXPECT_EQ(held(settings, kNew), 7);
    EXPECT_EQ(settings.find(kOld), nullptr);
    EXPECT_EQ(options.get_int(kNew), 1);
    EXPECT_TRUE(heard.empty());
}

TEST_F(SettingTest, Value)
{
    settings.set(kNew, OptionValue(5), env.stream());
    EXPECT_EQ(held(settings, kNew), 5);
    EXPECT_EQ(failure([&] { settings.set(kNew, OptionValue(2.0), env.stream()); }),
              "invalid argument: option 'new': is an int, not a double");
    EXPECT_EQ(held(settings, kNew), 5);
}

TEST_F(SettingTest, Erase)
{
    settings.set_int(kNew, 5, env.stream());
    settings.set_int(kOld, 6, env.stream());
    settings.erase(kNew);
    EXPECT_EQ(settings.find(kNew), nullptr);
    EXPECT_EQ(held(settings, kOld), 6);
    settings.erase(kNew);
    settings.erase(kGone);
    EXPECT_EQ(held(settings, kOld), 6);
    EXPECT_FALSE(settings.empty());
    settings.erase(kOld);
    EXPECT_TRUE(settings.empty());
}

TEST_F(SettingTest, Copy)
{
    static_assert(!std::is_copy_assignable_v<Settings>);
    settings.set_int(kNew, 5, env.stream());
    settings.set_string(kText, "replay.txt", env.stream());
    auto source = std::make_unique<Settings>(settings);
    const Settings copy(*source);
    source.reset();
    EXPECT_EQ(held(copy, kNew), 5);
    EXPECT_EQ(text(copy, kText), "replay.txt");
    EXPECT_NE(text(copy, kText).data(), text(settings, kText).data());
    settings.set_int(kNew, 8, env.stream());
    EXPECT_EQ(held(copy, kNew), 5);
}

TEST_F(SettingTest, Kept)
{
    settings.set_string(kText, "replay.txt", env.stream());
    const std::string_view first = text(settings, kText);
    settings.set_string(kText, "other.txt", env.stream());
    settings.set_int(kNew, 5, env.stream());
    settings.set_int(kOld, 6, env.stream());
    EXPECT_EQ(first, "replay.txt");
    EXPECT_EQ(first.data()[first.size()], '\0');
    settings.set_string(kText, std::string("replay.txt"), env.stream());
    EXPECT_EQ(text(settings, kText).data(), first.data());
    settings.erase(kText);
    settings.set_string(kText, "replay.txt", env.stream());
    EXPECT_EQ(text(settings, kText).data(), first.data());
    EXPECT_EQ(first, "replay.txt");
}

TEST_F(SettingTest, Scope)
{
    const std::string scope = "invalid argument: option 'host': has environment scope";
    EXPECT_EQ(failure([&] { settings.set_int(kHost, 3, env.stream()); }), scope);
    EXPECT_EQ(failure([&] { settings.set_int(kHost, -1, env.stream()); }), scope);
    EXPECT_EQ(failure([&] { settings.set_double(kHost, 2.0, env.stream()); }), scope);
    EXPECT_EQ(failure([&] { static_cast<void>(settings.find(kHost)); }), scope);
    EXPECT_EQ(failure([&] { settings.erase(kHost); }), scope);
    options.set_int(kHost, 3);
    EXPECT_EQ(options.get_int(kHost), 3);
}

TEST_F(SettingTest, Refusal)
{
    settings.set_int(kNew, 5, env.stream());
    const std::string range = "invalid argument: option 'new': -1 is outside [0, 2147483647]";
    EXPECT_EQ(failure([&] { settings.set_int(kNew, -1, env.stream()); }), range);
    EXPECT_EQ(failure([&] { options.set_int(kNew, -1); }), range);
    const std::string kind = "invalid argument: option 'new': is an int, not a double";
    EXPECT_EQ(failure([&] { settings.set_double(kNew, 2.0, env.stream()); }), kind);
    EXPECT_EQ(failure([&] { options.set_double(kNew, 2.0); }), kind);
    const std::string retired = "invalid argument: option 'gone': retired in 0.0.3, use 'new'";
    EXPECT_EQ(failure([&] { settings.set_int(kGone, 2, env.stream()); }), retired);
    EXPECT_EQ(failure([&] { options.set_int(kGone, 2); }), retired);
    EXPECT_EQ(held(settings, kNew), 5);
    EXPECT_EQ(settings.find(kGone), nullptr);
    EXPECT_TRUE(heard.empty());
}

TEST_F(SettingTest, Deprecated)
{
    settings.set_int(kOld, 5, env.stream());
    options.set_int(kOld, 6);
    EXPECT_EQ(held(settings, kOld), 5);
    ASSERT_EQ(heard.size(), 2U);
    for (const Event &event : heard)
    {
        EXPECT_EQ(event.model, nullptr);
        EXPECT_EQ(event.kind, EventKind::Message);
        EXPECT_EQ(event.level, EventLevel::Warning);
        EXPECT_EQ(event.module, "core");
        EXPECT_EQ(event.text, "option 'old': deprecated in 0.0.2, use 'new'");
        EXPECT_EQ(event.fields.size(), 3U);
    }
}

TEST_F(LookupTest, Sources)
{
    EXPECT_EQ(found(kNew), 1);
    EXPECT_EQ(source(kNew), OptionSource::Default);
    options.set_int(kNew, 3);
    EXPECT_EQ(found(kNew), 3);
    EXPECT_EQ(source(kNew), OptionSource::Environment);
    settings.set_int(kNew, 5, env.stream());
    EXPECT_EQ(found(kNew), 5);
    EXPECT_EQ(source(kNew), OptionSource::Model);
    EXPECT_EQ(lookup.get_int(kNew), 5);
    EXPECT_EQ(source(kOld), OptionSource::Default);
}

TEST_F(LookupTest, Erase)
{
    options.set_int(kNew, 3);
    settings.set_int(kNew, 5, env.stream());
    settings.erase(kNew);
    EXPECT_EQ(found(kNew), 3);
    EXPECT_EQ(source(kNew), OptionSource::Environment);
}

TEST_F(LookupTest, Shared)
{
    options.set_int(kNew, 3);
    EXPECT_EQ(lookup.get_int(kNew), 3);
    options.set_int(kNew, 4);
    EXPECT_EQ(lookup.get_int(kNew), 4);
    EXPECT_EQ(source(kNew), OptionSource::Environment);
}

TEST_F(LookupTest, Scope)
{
    const std::string scope = "invalid argument: option 'host': has environment scope";
    options.set_int(kHost, 6);
    EXPECT_EQ(failure([&] { static_cast<void>(lookup.find(kHost)); }), scope);
    EXPECT_EQ(failure([&] { static_cast<void>(lookup.get_int(kHost)); }), scope);
    EXPECT_EQ(failure([&] { static_cast<void>(lookup.get_double(kHost)); }), scope);
    const Lookup shared(options);
    EXPECT_EQ(shared.get_int(kHost), 6);
    EXPECT_EQ(shared.find(kHost).source, OptionSource::Environment);
}

TEST_F(LookupTest, Models)
{
    Settings other{kRows};
    const Lookup second(other, options);
    const Lookup shared(options);
    options.set_int(kNew, 3);
    settings.set_int(kNew, 5, env.stream());
    EXPECT_EQ(lookup.get_int(kNew), 5);
    EXPECT_EQ(second.get_int(kNew), 3);
    EXPECT_EQ(shared.get_int(kNew), 3);
    EXPECT_EQ(shared.find(kNew).source, OptionSource::Environment);
}

TEST_F(LookupTest, MissingRow)
{
    EXPECT_EQ(failure([&] { static_cast<void>(lookup.find(Option{6})); }),
              "internal error: option 6: has no table row");
    EXPECT_EQ(failure([&] { static_cast<void>(lookup.get_int(Option{0})); }),
              "internal error: option 0: has no table row");
}

TEST(GetterTest, Kinds)
{
    using biyos::Checking;
    Env env;
    Options options(env);
    Settings settings;
    const Lookup lookup(settings, options);
    settings.set_bool(Option::Presolve, false, env.stream());
    settings.set_int(Option::Seed, 4, env.stream());
    settings.set_double(Option::TimeLimit, 2.5, env.stream());
    settings.set_string(Option::Reproducer, "replay.txt", env.stream());
    settings.set_int(Option::Checking, BIYOS_CHK_FULL, env.stream());
    EXPECT_FALSE(lookup.get_bool(Option::Presolve));
    EXPECT_EQ(lookup.get_int(Option::Seed), 4);
    EXPECT_DOUBLE_EQ(lookup.get_double(Option::TimeLimit), 2.5);
    EXPECT_EQ(lookup.get_string(Option::Reproducer), "replay.txt");
    EXPECT_EQ(lookup.get_int(Option::Checking), BIYOS_CHK_FULL);
    EXPECT_EQ(lookup.get_enum<Checking>(Option::Checking), Checking::Full);
    EXPECT_EQ(options.get_enum<Checking>(Option::Checking), Checking::Cheap);
    EXPECT_EQ(failure([&] { static_cast<void>(lookup.get_int(Option::TimeLimit)); }),
              "invalid argument: option 'maxtime': is a double, not an int");
    EXPECT_EQ(failure([&] { static_cast<void>(lookup.get_bool(Option::Seed)); }),
              "invalid argument: option 'seed': is an int, not a bool");
    EXPECT_EQ(failure([&] { static_cast<void>(lookup.get_double(Option::Seed)); }),
              "invalid argument: option 'seed': is an int, not a double");
    EXPECT_EQ(failure([&] { static_cast<void>(lookup.get_string(Option::Seed)); }),
              "invalid argument: option 'seed': is an int, not a string");
    EXPECT_EQ(failure([&] { static_cast<void>(lookup.get_enum<Checking>(Option::Seed)); }),
              "invalid argument: option 'seed': is an int, not an enum");
    EXPECT_EQ(failure([&] { static_cast<void>(lookup.get_string(Option::Checking)); }),
              "invalid argument: option 'checking': is an enum, not a string");
}

TEST(GetterTest, Enum)
{
    using biyos::Checking;
    Env env;
    Options options(env);
    Settings settings;
    const Lookup lookup(settings, options);
    settings.set_int(Option::Checking, BIYOS_CHK_NONE, env.stream());
    EXPECT_EQ(failure([&] { settings.set_int(Option::Checking, 4, env.stream()); }),
              "invalid argument: option 'checking': 4 is not one of 1 (none), 2 (cheap), 3 (full)");
    EXPECT_EQ(failure([&] { settings.set_string(Option::Checking, "full", env.stream()); }),
              "invalid argument: option 'checking': is an enum, not a string");
    EXPECT_EQ(lookup.get_enum<Checking>(Option::Checking), Checking::None);
}
