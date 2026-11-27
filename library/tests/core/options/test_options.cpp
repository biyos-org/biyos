#include "core/env/env.hpp"
#include "core/env/error.hpp"
#include "core/env/version.hpp"
#include "core/events/events.hpp"
#include "core/numbers.hpp"
#include "core/options/options.hpp"
#include "core/options/table.hpp"
#include "core/timing/clock.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace
{

using biyos::EnumRule;
using biyos::EnumValue;
using biyos::Env;
using biyos::Error;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::Field;
using biyos::find_option;
using biyos::IntRule;
using biyos::kind_of;
using biyos::name_of;
using biyos::Option;
using biyos::option_table;
using biyos::Options;
using biyos::OptionSpec;
using biyos::Real;
using biyos::Scope;
using biyos::SystemClock;
using biyos::version;
using biyos::Version;
using biyos::versions_of;
using Rows = std::array<OptionSpec, 3>;

constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
constexpr Real kNan = std::numeric_limits<Real>::quiet_NaN();
constexpr std::string_view kMalformed = "internal error: option table: is not well formed";
constexpr Version kFirst{0, 0, 1};
constexpr Version kSecond{0, 0, 2};
constexpr Version kThird{0, 0, 3};
constexpr std::size_t kHardware = 8;

constexpr std::array kLevels{
    EnumValue{.number = 1, .name = "low"},
    EnumValue{.number = 2, .name = "high"},
};
constexpr std::array kGapped{
    EnumValue{.number = 1, .name = "low"},
    EnumValue{.number = 3, .name = "high"},
};
constexpr std::array kZeroed{
    EnumValue{.number = 0, .name = "low"},
    EnumValue{.number = 1, .name = "high"},
};
constexpr std::array kTwins{
    EnumValue{.number = 1, .name = "low"},
    EnumValue{.number = 2, .name = "low"},
};

constexpr Rows kRows{
    OptionSpec{
        .id = Option{1},
        .name = "new",
        .scope = Scope::Model,
        .rule = IntRule{.default_value = 1, .lower = 0},
        .doc = "Current.",
        .versions = {.since = kFirst},
    },
    OptionSpec{
        .id = Option{2},
        .name = "old",
        .scope = Scope::Model,
        .rule = IntRule{.default_value = 1, .lower = 0},
        .doc = "Deprecated.",
        .versions = {.since = kFirst, .deprecated = kSecond},
        .replacement = Option{1},
    },
    OptionSpec{
        .id = Option{3},
        .name = "gone",
        .scope = Scope::Model,
        .rule = IntRule{.default_value = 1, .lower = 0},
        .doc = "Retired.",
        .versions = {.since = kFirst, .deprecated = kSecond, .retired = kThird},
        .replacement = Option{1},
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

struct Listener
{
    Listener() : Listener(kHardware) {}
    explicit Listener(std::size_t hardware) : env(std::make_unique<SystemClock>(), hardware)
    {
        const auto record = [this](const Event &event) { heard.push_back(event); };
        env.events().add_hook(EventKind::Message, EventLevel::Debug, record);
    }
    Listener(const Listener &) = delete;
    Listener &operator=(const Listener &) = delete;

    Env env;
    std::vector<Event> heard;
};

std::string built(const std::function<void(Rows &)> &change)
{
    Rows rows = kRows;
    change(rows);
    Listener listener;
    return failure([&] { const Options options(rows, listener.env); });
}

std::string built_enum(int default_value, std::span<const EnumValue> values)
{
    const EnumRule rule{.default_value = default_value, .values = values};
    return built([&rule](Rows &rows) { rows[0].rule = rule; });
}

std::string listed(const std::vector<Field> &fields)
{
    std::string text;
    for (const Field &field : fields)
    {
        text += (text.empty() ? "" : " ") + field.name + "=" + field.value;
    }
    return text;
}

}

TEST(OptionTest, Defaults)
{
    Listener listener;
    const Options options(listener.env);
    EXPECT_EQ(options.get_int(Option::Seed), 0);
    EXPECT_DOUBLE_EQ(options.get_double(Option::TimeLimit), kInfinity);
    EXPECT_EQ(options.get_int(Option::Checking), BIYOS_CHK_CHEAP);
    EXPECT_TRUE(options.get_bool(Option::Presolve));
    EXPECT_EQ(options.get_string(Option::Reproducer), "");
    EXPECT_DOUBLE_EQ(options.get_double(Option::FeasAbs), 1e-6);
    EXPECT_DOUBLE_EQ(options.get_double(Option::FeasRel), 1e-6);
    EXPECT_DOUBLE_EQ(options.get_double(Option::IntTol), 1e-5);
    EXPECT_DOUBLE_EQ(options.get_double(Option::ZeroTol), 1e-9);
    EXPECT_DOUBLE_EQ(options.get_double(Option::CpuTimeLimit), kInfinity);
    EXPECT_EQ(options.get_int(Option::Threads), 1);
}

TEST(OptionTest, Bool)
{
    Listener listener;
    Options options(listener.env);
    options.set_bool(Option::Presolve, false);
    EXPECT_FALSE(options.get_bool(Option::Presolve));
}

TEST(OptionTest, Int)
{
    Listener listener;
    Options options(listener.env);
    options.set_int(Option::Seed, 42);
    EXPECT_EQ(options.get_int(Option::Seed), 42);
    options.set_int(Option::Seed, (std::numeric_limits<int>::max)());
    EXPECT_EQ(options.get_int(Option::Seed), (std::numeric_limits<int>::max)());
}

TEST(OptionTest, IntRange)
{
    Listener listener;
    Options options(listener.env);
    options.set_int(Option::Seed, 7);
    EXPECT_EQ(failure([&] { options.set_int(Option::Seed, -1); }),
              "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(options.get_int(Option::Seed), 7);
}

TEST(OptionTest, Double)
{
    Listener listener;
    Options options(listener.env);
    options.set_double(Option::TimeLimit, 2.5);
    EXPECT_DOUBLE_EQ(options.get_double(Option::TimeLimit), 2.5);
    options.set_double(Option::TimeLimit, 0.0);
    EXPECT_DOUBLE_EQ(options.get_double(Option::TimeLimit), 0.0);
    options.set_double(Option::TimeLimit, kInfinity);
    EXPECT_DOUBLE_EQ(options.get_double(Option::TimeLimit), kInfinity);
}

TEST(OptionTest, DoubleRange)
{
    Listener listener;
    Options options(listener.env);
    options.set_double(Option::TimeLimit, 2.5);
    EXPECT_EQ(failure([&] { options.set_double(Option::TimeLimit, -0.5); }),
              "invalid argument: option 'maxtime': -0.5 is outside [0, inf]");
    EXPECT_EQ(failure([&] { options.set_double(Option::TimeLimit, kNan); }),
              "invalid argument: option 'maxtime': nan is outside [0, inf]");
    EXPECT_DOUBLE_EQ(options.get_double(Option::TimeLimit), 2.5);
}

TEST(OptionTest, CpuLimit)
{
    Listener listener;
    Options options(listener.env);
    options.set_double(Option::CpuTimeLimit, 0.0);
    EXPECT_DOUBLE_EQ(options.get_double(Option::CpuTimeLimit), 0.0);
    options.set_double(Option::CpuTimeLimit, 2.5);
    EXPECT_EQ(failure([&] { options.set_double(Option::CpuTimeLimit, -0.5); }),
              "invalid argument: option 'maxcputime': -0.5 is outside [0, inf]");
    EXPECT_EQ(failure([&] { options.set_double(Option::CpuTimeLimit, kNan); }),
              "invalid argument: option 'maxcputime': nan is outside [0, inf]");
    EXPECT_DOUBLE_EQ(options.get_double(Option::CpuTimeLimit), 2.5);
}

TEST(OptionTest, Threads)
{
    Listener listener;
    Options options(listener.env);
    options.set_int(Option::Threads, 8);
    EXPECT_EQ(failure([&] { options.set_int(Option::Threads, 0); }),
              "invalid argument: option 'threads': 0 is outside [1, 2147483647]");
    EXPECT_EQ(options.get_int(Option::Threads), 8);
}

TEST(OptionTest, Capped)
{
    Listener listener(4);
    Options options(listener.env);
    options.set_int(Option::Threads, 4);
    EXPECT_TRUE(listener.heard.empty());
    options.set_int(Option::Threads, 8);
    EXPECT_EQ(options.get_int(Option::Threads), 8);
    ASSERT_EQ(listener.heard.size(), 1U);
    const Event &event = listener.heard.front();
    EXPECT_EQ(event.kind, EventKind::Message);
    EXPECT_EQ(event.level, EventLevel::Warning);
    EXPECT_EQ(event.module, "core");
    EXPECT_EQ(event.text, "option 'threads': 8 is above the 4 hardware threads, so 4 run");
    EXPECT_EQ(listed(event.fields), "option=threads value=8 hardware=4");
}

TEST(OptionTest, ToleranceRange)
{
    Listener listener;
    Options options(listener.env);
    for (const Option option : {Option::FeasAbs, Option::FeasRel, Option::IntTol, Option::ZeroTol})
    {
        const std::string name(name_of(option));
        options.set_double(option, 0.0);
        EXPECT_DOUBLE_EQ(options.get_double(option), 0.0);
        options.set_double(option, kInfinity);
        EXPECT_DOUBLE_EQ(options.get_double(option), kInfinity);
        options.set_double(option, 1e-3);
        EXPECT_EQ(failure([&] { options.set_double(option, -1e-9); }),
                  "invalid argument: option '" + name + "': -1e-09 is outside [0, inf]");
        EXPECT_EQ(failure([&] { options.set_double(option, kNan); }),
                  "invalid argument: option '" + name + "': nan is outside [0, inf]");
        EXPECT_DOUBLE_EQ(options.get_double(option), 1e-3);
    }
}

TEST(OptionTest, String)
{
    Listener listener;
    Options options(listener.env);
    options.set_string(Option::Reproducer, "replay.txt");
    EXPECT_EQ(options.get_string(Option::Reproducer), "replay.txt");
}

TEST(OptionTest, Kept)
{
    Listener listener;
    Options options(listener.env);
    const std::string_view empty = options.get_string(Option::Reproducer);
    options.set_string(Option::Reproducer, "replay.txt");
    const std::string_view first = options.get_string(Option::Reproducer);
    options.set_string(Option::Reproducer, "other.txt");
    options.set_int(Option::Seed, 3);
    EXPECT_EQ(first, "replay.txt");
    EXPECT_EQ(first.data()[first.size()], '\0');
    EXPECT_EQ(empty, "");
    options.set_string(Option::Reproducer, std::string("replay.txt"));
    EXPECT_EQ(options.get_string(Option::Reproducer).data(), first.data());
    options.set_string(Option::Reproducer, "");
    EXPECT_EQ(options.get_string(Option::Reproducer).data(), empty.data());
}

TEST(OptionTest, Enum)
{
    Listener listener;
    Options options(listener.env);
    for (const int checking : {BIYOS_CHK_FULL, BIYOS_CHK_NONE, BIYOS_CHK_CHEAP})
    {
        options.set_int(Option::Checking, checking);
        EXPECT_EQ(options.get_int(Option::Checking), checking);
    }
}

TEST(OptionTest, EnumValue)
{
    Listener listener;
    Options options(listener.env);
    options.set_int(Option::Checking, BIYOS_CHK_FULL);
    const std::string option = "invalid argument: option 'checking': ";
    const std::string list = " is not one of 1 (none), 2 (cheap), 3 (full)";
    EXPECT_EQ(failure([&] { options.set_int(Option::Checking, 4); }), option + "4" + list);
    EXPECT_EQ(failure([&] { options.set_int(Option::Checking, 0); }), option + "0" + list);
    EXPECT_EQ(failure([&] { options.set_int(Option::Checking, -1); }), option + "-1" + list);
    EXPECT_EQ(options.get_int(Option::Checking), BIYOS_CHK_FULL);
}

TEST(OptionTest, TypedEnum)
{
    using biyos::Checking;
    Listener listener;
    Options options(listener.env);
    EXPECT_EQ(options.get_enum<Checking>(Option::Checking), Checking::Cheap);
    options.set_int(Option::Checking, BIYOS_CHK_FULL);
    EXPECT_EQ(options.get_enum<Checking>(Option::Checking), Checking::Full);
    options.set_int(Option::Checking, BIYOS_CHK_NONE);
    EXPECT_EQ(options.get_enum<Checking>(Option::Checking), Checking::None);
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_enum<Checking>(Option::Seed)); }),
              "invalid argument: option 'seed': is an int, not an enum");
}

TEST(OptionTest, WrongKind)
{
    Listener listener;
    Options options(listener.env);
    EXPECT_EQ(failure([&] { options.set_double(Option::Seed, 1.0); }),
              "invalid argument: option 'seed': is an int, not a double");
    EXPECT_EQ(failure([&] { options.set_string(Option::Presolve, "yes"); }),
              "invalid argument: option 'presolve': is a bool, not a string");
    EXPECT_EQ(failure([&] { options.set_string(Option::Checking, "full"); }),
              "invalid argument: option 'checking': is an enum, not a string");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_string(Option::Checking)); }),
              "invalid argument: option 'checking': is an enum, not a string");
    EXPECT_EQ(failure([&] { options.set_double(Option::Checking, 2.0); }),
              "invalid argument: option 'checking': is an enum, not a double");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_int(Option::Reproducer)); }),
              "invalid argument: option 'repro': is a string, not an int");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_bool(Option::Reproducer)); }),
              "invalid argument: option 'repro': is a string, not a bool");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_int(Option::TimeLimit)); }),
              "invalid argument: option 'maxtime': is a double, not an int");
}

TEST(OptionTest, Names)
{
    for (const Option option : {Option::Seed,
                                Option::TimeLimit,
                                Option::Checking,
                                Option::Presolve,
                                Option::Reproducer,
                                Option::FeasAbs,
                                Option::FeasRel,
                                Option::IntTol,
                                Option::ZeroTol,
                                Option::CpuTimeLimit,
                                Option::Threads})
    {
        EXPECT_EQ(find_option(name_of(option)), option);
        EXPECT_EQ(find_option(static_cast<int>(option)), option);
    }
    EXPECT_EQ(name_of(Option::TimeLimit), "maxtime");
}

TEST(OptionTest, Kinds)
{
    using biyos::OptionKind;
    EXPECT_EQ(kind_of(Option::Seed), OptionKind::Int);
    EXPECT_EQ(kind_of(Option::TimeLimit), OptionKind::Double);
    EXPECT_EQ(kind_of(Option::Checking), OptionKind::Enum);
    EXPECT_EQ(kind_of(Option::Presolve), OptionKind::Bool);
    EXPECT_EQ(kind_of(Option::Reproducer), OptionKind::String);
    EXPECT_EQ(kind_of(Option::FeasAbs), OptionKind::Double);
    EXPECT_EQ(kind_of(Option::FeasRel), OptionKind::Double);
    EXPECT_EQ(kind_of(Option::IntTol), OptionKind::Double);
    EXPECT_EQ(kind_of(Option::ZeroTol), OptionKind::Double);
    EXPECT_EQ(kind_of(Option::CpuTimeLimit), OptionKind::Double);
    EXPECT_EQ(kind_of(Option::Threads), OptionKind::Int);
}

TEST(OptionTest, Versions)
{
    const biyos::Versions seed = versions_of(Option::Seed);
    EXPECT_EQ(seed.since, (Version{0, 1, 0}));
    EXPECT_FALSE(seed.deprecated);
    EXPECT_FALSE(seed.retired);
}

TEST(OptionTest, UnknownName)
{
    EXPECT_EQ(failure([] { find_option("seeds"); }), "invalid argument: option 'seeds': unknown");
    EXPECT_EQ(failure([] { find_option(""); }), "invalid argument: option '': unknown");
}

TEST(OptionTest, UnknownId)
{
    EXPECT_EQ(failure([] { find_option(0); }), "invalid argument: option 0: unknown");
    EXPECT_EQ(failure([] { find_option(12); }), "invalid argument: option 12: unknown");
    EXPECT_EQ(failure([] { find_option(-3); }), "invalid argument: option -3: unknown");
}

TEST(OptionTest, MissingRow)
{
    Listener listener;
    Options options(listener.env);
    const Option past = static_cast<Option>(12);
    const Option zero = static_cast<Option>(0);
    EXPECT_EQ(failure([&] { options.set_int(past, 1); }),
              "internal error: option 12: has no table row");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_string(zero)); }),
              "internal error: option 0: has no table row");
    EXPECT_EQ(failure([&] { static_cast<void>(name_of(past)); }),
              "internal error: option 12: has no table row");
    EXPECT_EQ(failure([&] { static_cast<void>(kind_of(past)); }),
              "internal error: option 12: has no table row");
    EXPECT_EQ(failure([&] { static_cast<void>(versions_of(past)); }),
              "internal error: option 12: has no table row");
}

TEST(OptionTest, Sources)
{
    using biyos::OptionSource;
    Listener listener;
    Options options(listener.env);
    EXPECT_EQ(options.source_of(Option::Seed), OptionSource::Default);
    options.set_int(Option::Seed, 3);
    EXPECT_EQ(options.source_of(Option::Seed), OptionSource::Environment);
    EXPECT_EQ(options.source_of(Option::TimeLimit), OptionSource::Default);
    static_cast<void>(failure([&] { options.set_double(Option::TimeLimit, -1.0); }));
    EXPECT_EQ(options.source_of(Option::TimeLimit), OptionSource::Default);
    EXPECT_EQ(std::get<int>(options.value_of(Option::Seed)), 3);
    EXPECT_EQ(failure([&] { static_cast<void>(options.source_of(Option{12})); }),
              "internal error: option 12: has no table row");
    EXPECT_EQ(failure([&] { static_cast<void>(options.value_of(Option{0})); }),
              "internal error: option 0: has no table row");
}

TEST(OptionTest, Drop)
{
    using biyos::OptionSource;
    Listener listener;
    Options options(listener.env);
    options.set_int(Option::Seed, 3);
    options.drop(Option::Seed);
    EXPECT_EQ(options.get_int(Option::Seed), 0);
    EXPECT_EQ(options.source_of(Option::Seed), OptionSource::Default);
    options.drop(Option::TimeLimit);
    EXPECT_EQ(options.source_of(Option::TimeLimit), OptionSource::Default);
    EXPECT_EQ(failure([&] { options.drop(Option{12}); }),
              "internal error: option 12: has no table row");
}

TEST(OptionTest, DropKept)
{
    Listener listener;
    Options options(listener.env);
    options.set_string(Option::Reproducer, "replay.txt");
    const std::string_view held = options.get_string(Option::Reproducer);
    options.drop(Option::Reproducer);
    EXPECT_EQ(options.get_string(Option::Reproducer), "");
    EXPECT_EQ(held, "replay.txt");
    EXPECT_EQ(held.data()[held.size()], '\0');
}

TEST(OptionTest, DropSilent)
{
    Listener listener;
    Options options(kRows, listener.env);
    options.drop(Option{2});
    EXPECT_TRUE(listener.heard.empty());
}

TEST(OptionTest, Table)
{
    Listener listener;
    Options options(kRows, listener.env);
    EXPECT_EQ(options.get_int(Option{1}), 1);
    options.set_int(Option{1}, 5);
    EXPECT_EQ(options.get_int(Option{1}), 5);
    EXPECT_EQ(failure([&] { options.set_int(Option{1}, -1); }),
              "invalid argument: option 'new': -1 is outside [0, 2147483647]");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_int(Option{4})); }),
              "internal error: option 4: has no table row");
    EXPECT_TRUE(listener.heard.empty());
}

TEST(OptionTest, BadTable)
{
    EXPECT_EQ(built([](Rows &) {}), "no error");
    EXPECT_EQ(built([](Rows &rows) { rows[1].id = Option{3}; }), kMalformed);
    EXPECT_EQ(built([](Rows &rows) { rows[1].name = "new"; }), kMalformed);
    const IntRule below{.default_value = -1, .lower = 0};
    EXPECT_EQ(built([&below](Rows &rows) { rows[1].rule = below; }), kMalformed);
}

TEST(OptionTest, BadEnum)
{
    EXPECT_EQ(built_enum(2, kLevels), "no error");
    EXPECT_EQ(built_enum(3, kLevels), kMalformed);
    EXPECT_EQ(built_enum(1, {}), kMalformed);
    EXPECT_EQ(built_enum(1, kGapped), kMalformed);
    EXPECT_EQ(built_enum(1, kZeroed), kMalformed);
    EXPECT_EQ(built_enum(1, kTwins), kMalformed);
}

TEST(OptionTest, Scopes)
{
    EXPECT_EQ(built([](Rows &rows) { rows[0].scope = Scope::Environment; }), "no error");
    EXPECT_EQ(built([](Rows &rows) { rows[0].scope = Scope{}; }), kMalformed);
    EXPECT_EQ(built([](Rows &rows) { rows[2].scope = Scope{3}; }), kMalformed);
    for (const OptionSpec &spec : option_table())
    {
        const Scope scope = spec.id == Option::Threads ? Scope::Environment : Scope::Model;
        EXPECT_EQ(spec.scope, scope) << spec.name;
    }
}

TEST(OptionTest, Dates)
{
    const Version project = version();
    const Version later{project.major + 1, 0, 0};
    EXPECT_EQ(built([&](Rows &rows) { rows[2].versions.retired = project; }), "no error");
    EXPECT_EQ(built([](Rows &rows) { rows[0].versions.since = Version{}; }), kMalformed);
    EXPECT_EQ(built([&](Rows &rows) { rows[0].versions.since = later; }), kMalformed);
    EXPECT_EQ(built([](Rows &rows) { rows[1].versions.deprecated = kFirst; }), kMalformed);
    EXPECT_EQ(built([&](Rows &rows) { rows[1].versions.deprecated = later; }), kMalformed);
    EXPECT_EQ(built([](Rows &rows) { rows[2].versions.retired = kSecond; }), kMalformed);
    EXPECT_EQ(built([&](Rows &rows) { rows[2].versions.retired = later; }), kMalformed);
    EXPECT_EQ(built([](Rows &rows) { rows[0].versions.retired = kThird; }), kMalformed);
    EXPECT_EQ(built([](Rows &rows) { rows[2].versions.retired = Version{0, 0, 100}; }), kMalformed);
    EXPECT_EQ(built([](Rows &rows) { rows[2].versions.retired = Version{0, 1, -1}; }), kMalformed);
}

TEST(OptionTest, Replacements)
{
    EXPECT_EQ(built([](Rows &rows) { rows[1].replacement.reset(); }), kMalformed);
    EXPECT_EQ(built([](Rows &rows) { rows[0].replacement = Option{1}; }), kMalformed);
    EXPECT_EQ(built([](Rows &rows) { rows[1].replacement = Option{2}; }), kMalformed);
    EXPECT_EQ(built([](Rows &rows) { rows[1].replacement = Option{3}; }), kMalformed);
    EXPECT_EQ(built([](Rows &rows) { rows[1].replacement = Option{4}; }), kMalformed);
    EXPECT_EQ(built([](Rows &rows) { rows[1].replacement = Option{0}; }), kMalformed);
}

TEST(OptionTest, Retired)
{
    const std::string retired = "invalid argument: option 'gone': retired in 0.0.3, use 'new'";
    Listener listener;
    Options options(kRows, listener.env);
    EXPECT_EQ(failure([&] { options.set_int(Option{3}, 5); }), retired);
    EXPECT_EQ(failure([&] { options.set_bool(Option{3}, true); }), retired);
    EXPECT_EQ(failure([&] { options.set_double(Option{3}, 5.0); }), retired);
    EXPECT_EQ(failure([&] { options.set_string(Option{3}, "5"); }), retired);
    EXPECT_EQ(options.get_int(Option{3}), 1);
    EXPECT_TRUE(listener.heard.empty());
}

TEST(OptionTest, Deprecated)
{
    Listener listener;
    Options options(kRows, listener.env);
    options.set_int(Option{2}, 5);
    EXPECT_EQ(options.get_int(Option{2}), 5);
    ASSERT_EQ(listener.heard.size(), 1U);
    const Event &event = listener.heard.front();
    EXPECT_EQ(event.kind, EventKind::Message);
    EXPECT_EQ(event.level, EventLevel::Warning);
    EXPECT_EQ(event.module, "core");
    EXPECT_EQ(event.text, "option 'old': deprecated in 0.0.2, use 'new'");
    EXPECT_EQ(listed(event.fields), "option=old deprecated=0.0.2 replacement=new");
    EXPECT_EQ(failure([&] { options.set_int(Option{2}, -1); }),
              "invalid argument: option 'old': -1 is outside [0, 2147483647]");
    EXPECT_EQ(listener.heard.size(), 1U);
}
