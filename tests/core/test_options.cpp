#include "core/error.hpp"
#include "core/options.hpp"
#include "core/version.hpp"

#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <limits>
#include <string>
#include <string_view>

namespace
{

using biyos::IntRule;
using biyos::Option;
using biyos::OptionSpec;
using biyos::Version;
using Rows = std::array<OptionSpec, 3>;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr std::string_view kMalformed = "internal error: option table: is not well formed";
constexpr Version kFirst{0, 0, 1};
constexpr Version kSecond{0, 0, 2};
constexpr Version kThird{0, 0, 3};

constexpr Rows kRows{
    OptionSpec{
        .id = Option{1},
        .name = "new",
        .rule = IntRule{.default_value = 1, .lower = 0},
        .doc = "Current.",
        .versions = {.since = kFirst},
    },
    OptionSpec{
        .id = Option{2},
        .name = "old",
        .rule = IntRule{.default_value = 1, .lower = 0},
        .doc = "Deprecated.",
        .versions = {.since = kFirst, .deprecated = kSecond},
        .replacement = Option{1},
    },
    OptionSpec{
        .id = Option{3},
        .name = "gone",
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
    catch (const biyos::Error &error)
    {
        return error.what();
    }
    return "no error";
}

std::string built(const std::function<void(Rows &)> &change)
{
    Rows rows = kRows;
    change(rows);
    return failure([&rows] { const biyos::Options options(rows); });
}

}

TEST(OptionTest, Defaults)
{
    const biyos::Options options;
    EXPECT_EQ(options.get_int(Option::Seed), 0);
    EXPECT_DOUBLE_EQ(options.get_double(Option::TimeLimit), kInfinity);
    EXPECT_EQ(options.get_string(Option::CheckLevel), "cheap");
    EXPECT_TRUE(options.get_bool(Option::Presolve));
    EXPECT_EQ(options.get_string(Option::Reproducer), "");
    EXPECT_DOUBLE_EQ(options.get_double(Option::FeasibilityAbs), 1e-6);
    EXPECT_DOUBLE_EQ(options.get_double(Option::FeasibilityRel), 1e-6);
    EXPECT_DOUBLE_EQ(options.get_double(Option::IntegralityTol), 1e-5);
    EXPECT_DOUBLE_EQ(options.get_double(Option::ZeroTol), 1e-9);
    EXPECT_DOUBLE_EQ(options.get_double(Option::CpuTimeLimit), kInfinity);
}

TEST(OptionTest, Bool)
{
    biyos::Options options;
    options.set_bool(Option::Presolve, false);
    EXPECT_FALSE(options.get_bool(Option::Presolve));
}

TEST(OptionTest, Int)
{
    biyos::Options options;
    options.set_int(Option::Seed, 42);
    EXPECT_EQ(options.get_int(Option::Seed), 42);
    options.set_int(Option::Seed, (std::numeric_limits<int>::max)());
    EXPECT_EQ(options.get_int(Option::Seed), (std::numeric_limits<int>::max)());
}

TEST(OptionTest, IntRange)
{
    biyos::Options options;
    options.set_int(Option::Seed, 7);
    EXPECT_EQ(failure([&] { options.set_int(Option::Seed, -1); }),
              "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(options.get_int(Option::Seed), 7);
}

TEST(OptionTest, Double)
{
    biyos::Options options;
    options.set_double(Option::TimeLimit, 2.5);
    EXPECT_DOUBLE_EQ(options.get_double(Option::TimeLimit), 2.5);
    options.set_double(Option::TimeLimit, 0.0);
    EXPECT_DOUBLE_EQ(options.get_double(Option::TimeLimit), 0.0);
    options.set_double(Option::TimeLimit, kInfinity);
    EXPECT_DOUBLE_EQ(options.get_double(Option::TimeLimit), kInfinity);
}

TEST(OptionTest, DoubleRange)
{
    biyos::Options options;
    options.set_double(Option::TimeLimit, 2.5);
    EXPECT_EQ(failure([&] { options.set_double(Option::TimeLimit, -0.5); }),
              "invalid argument: option 'time_limit': -0.5 is outside [0, inf]");
    EXPECT_EQ(failure([&] { options.set_double(Option::TimeLimit, kNan); }),
              "invalid argument: option 'time_limit': nan is outside [0, inf]");
    EXPECT_DOUBLE_EQ(options.get_double(Option::TimeLimit), 2.5);
}

TEST(OptionTest, CpuLimit)
{
    biyos::Options options;
    options.set_double(Option::CpuTimeLimit, 0.0);
    EXPECT_DOUBLE_EQ(options.get_double(Option::CpuTimeLimit), 0.0);
    options.set_double(Option::CpuTimeLimit, 2.5);
    EXPECT_EQ(failure([&] { options.set_double(Option::CpuTimeLimit, -0.5); }),
              "invalid argument: option 'cpu_time_limit': -0.5 is outside [0, inf]");
    EXPECT_EQ(failure([&] { options.set_double(Option::CpuTimeLimit, kNan); }),
              "invalid argument: option 'cpu_time_limit': nan is outside [0, inf]");
    EXPECT_DOUBLE_EQ(options.get_double(Option::CpuTimeLimit), 2.5);
}

TEST(OptionTest, ToleranceRange)
{
    biyos::Options options;
    for (const Option option :
         {Option::FeasibilityAbs, Option::FeasibilityRel, Option::IntegralityTol, Option::ZeroTol})
    {
        const std::string name(biyos::name_of(option));
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
    biyos::Options options;
    options.set_string(Option::Reproducer, "replay.txt");
    EXPECT_EQ(options.get_string(Option::Reproducer), "replay.txt");
}

TEST(OptionTest, Enum)
{
    biyos::Options options;
    options.set_string(Option::CheckLevel, "full");
    EXPECT_EQ(options.get_string(Option::CheckLevel), "full");
    options.set_string(Option::CheckLevel, "none");
    EXPECT_EQ(options.get_string(Option::CheckLevel), "none");
}

TEST(OptionTest, EnumValue)
{
    biyos::Options options;
    EXPECT_EQ(failure([&] { options.set_string(Option::CheckLevel, "most"); }),
              "invalid argument: option 'check_level': 'most' is not one of none, cheap, full");
    EXPECT_EQ(options.get_string(Option::CheckLevel), "cheap");
}

TEST(OptionTest, TypedEnum)
{
    using biyos::CheckLevel;
    biyos::Options options;
    EXPECT_EQ(options.get_enum<CheckLevel>(Option::CheckLevel), CheckLevel::Cheap);
    options.set_string(Option::CheckLevel, "full");
    EXPECT_EQ(options.get_enum<CheckLevel>(Option::CheckLevel), CheckLevel::Full);
    options.set_string(Option::CheckLevel, "none");
    EXPECT_EQ(options.get_enum<CheckLevel>(Option::CheckLevel), CheckLevel::None);
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_enum<CheckLevel>(Option::Seed)); }),
              "invalid argument: option 'seed': is an int, not an enum");
}

TEST(OptionTest, WrongKind)
{
    biyos::Options options;
    EXPECT_EQ(failure([&] { options.set_double(Option::Seed, 1.0); }),
              "invalid argument: option 'seed': is an int, not a double");
    EXPECT_EQ(failure([&] { options.set_string(Option::Presolve, "yes"); }),
              "invalid argument: option 'presolve': is a bool, not a string");
    EXPECT_EQ(failure([&] { options.set_int(Option::CheckLevel, 2); }),
              "invalid argument: option 'check_level': is an enum, not an int");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_bool(Option::Reproducer)); }),
              "invalid argument: option 'reproducer': is a string, not a bool");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_int(Option::TimeLimit)); }),
              "invalid argument: option 'time_limit': is a double, not an int");
}

TEST(OptionTest, Names)
{
    for (const Option option :
         {Option::Seed, Option::TimeLimit, Option::CheckLevel, Option::Presolve, Option::Reproducer,
          Option::FeasibilityAbs, Option::FeasibilityRel, Option::IntegralityTol, Option::ZeroTol,
          Option::CpuTimeLimit})
    {
        EXPECT_EQ(biyos::find_option(biyos::name_of(option)), option);
        EXPECT_EQ(biyos::find_option(static_cast<int>(option)), option);
    }
    EXPECT_EQ(biyos::name_of(Option::TimeLimit), "time_limit");
}

TEST(OptionTest, Kinds)
{
    using biyos::OptionKind;
    EXPECT_EQ(biyos::kind_of(Option::Seed), OptionKind::Int);
    EXPECT_EQ(biyos::kind_of(Option::TimeLimit), OptionKind::Double);
    EXPECT_EQ(biyos::kind_of(Option::CheckLevel), OptionKind::Enum);
    EXPECT_EQ(biyos::kind_of(Option::Presolve), OptionKind::Bool);
    EXPECT_EQ(biyos::kind_of(Option::Reproducer), OptionKind::String);
    EXPECT_EQ(biyos::kind_of(Option::FeasibilityAbs), OptionKind::Double);
    EXPECT_EQ(biyos::kind_of(Option::FeasibilityRel), OptionKind::Double);
    EXPECT_EQ(biyos::kind_of(Option::IntegralityTol), OptionKind::Double);
    EXPECT_EQ(biyos::kind_of(Option::ZeroTol), OptionKind::Double);
    EXPECT_EQ(biyos::kind_of(Option::CpuTimeLimit), OptionKind::Double);
}

TEST(OptionTest, UnknownName)
{
    EXPECT_EQ(failure([] { biyos::find_option("seeds"); }),
              "invalid argument: option 'seeds': unknown");
    EXPECT_EQ(failure([] { biyos::find_option(""); }), "invalid argument: option '': unknown");
}

TEST(OptionTest, UnknownId)
{
    EXPECT_EQ(failure([] { biyos::find_option(0); }), "invalid argument: option 0: unknown");
    EXPECT_EQ(failure([] { biyos::find_option(11); }), "invalid argument: option 11: unknown");
    EXPECT_EQ(failure([] { biyos::find_option(-3); }), "invalid argument: option -3: unknown");
}

TEST(OptionTest, MissingRow)
{
    biyos::Options options;
    const Option past = static_cast<Option>(11);
    const Option zero = static_cast<Option>(0);
    EXPECT_EQ(failure([&] { options.set_int(past, 1); }),
              "internal error: option 11: has no table row");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_string(zero)); }),
              "internal error: option 0: has no table row");
    EXPECT_EQ(failure([&] { static_cast<void>(biyos::name_of(past)); }),
              "internal error: option 11: has no table row");
    EXPECT_EQ(failure([&] { static_cast<void>(biyos::kind_of(past)); }),
              "internal error: option 11: has no table row");
}

TEST(OptionTest, Table)
{
    biyos::Options options(kRows);
    EXPECT_EQ(options.get_int(Option{1}), 1);
    options.set_int(Option{1}, 5);
    EXPECT_EQ(options.get_int(Option{1}), 5);
    EXPECT_EQ(failure([&] { options.set_int(Option{1}, -1); }),
              "invalid argument: option 'new': -1 is outside [0, 2147483647]");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_int(Option{4})); }),
              "internal error: option 4: has no table row");
}

TEST(OptionTest, BadTable)
{
    EXPECT_EQ(built([](Rows &) {}), "no error");
    EXPECT_EQ(built([](Rows &rows) { rows[1].id = Option{3}; }), kMalformed);
    EXPECT_EQ(built([](Rows &rows) { rows[1].name = "new"; }), kMalformed);
    const IntRule below{.default_value = -1, .lower = 0};
    EXPECT_EQ(built([&below](Rows &rows) { rows[1].rule = below; }), kMalformed);
}

TEST(OptionTest, Dates)
{
    const Version project = biyos::version();
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
