#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/events.hpp"
#include "core/options.hpp"
#include "core/version.hpp"

#include <gtest/gtest.h>

#include <array>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using biyos::EventKind;
using biyos::EventLevel;
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

struct Stream
{
    Stream()
    {
        const auto record = [this](const biyos::Event &evt) { heard.push_back(evt); };
        events.add_hook(EventKind::Message, EventLevel::Debug, record);
    }
    Stream(const Stream &) = delete;
    Stream &operator=(const Stream &) = delete;

    biyos::FakeClock clock;
    biyos::Events events{clock};
    std::vector<biyos::Event> heard;
};

std::string built(const std::function<void(Rows &)> &change)
{
    Rows rows = kRows;
    change(rows);
    Stream stream;
    return failure([&] { const biyos::Options opts(rows, stream.events); });
}

std::string listed(const std::vector<biyos::Field> &fields)
{
    std::string text;
    for (const biyos::Field &field : fields)
    {
        text += (text.empty() ? "" : " ") + field.name + "=" + field.value;
    }
    return text;
}

}

TEST(OptionTest, Defaults)
{
    Stream stream;
    const biyos::Options opts(stream.events);
    EXPECT_EQ(opts.get_int(Option::Seed), 0);
    EXPECT_DOUBLE_EQ(opts.get_dbl(Option::TimeLimit), kInfinity);
    EXPECT_EQ(opts.get_str(Option::CheckLevel), "cheap");
    EXPECT_TRUE(opts.get_bool(Option::Presolve));
    EXPECT_EQ(opts.get_str(Option::Reproducer), "");
    EXPECT_DOUBLE_EQ(opts.get_dbl(Option::FeasibilityAbs), 1e-6);
    EXPECT_DOUBLE_EQ(opts.get_dbl(Option::FeasibilityRel), 1e-6);
    EXPECT_DOUBLE_EQ(opts.get_dbl(Option::IntegralityTol), 1e-5);
    EXPECT_DOUBLE_EQ(opts.get_dbl(Option::ZeroTol), 1e-9);
    EXPECT_DOUBLE_EQ(opts.get_dbl(Option::CpuTimeLimit), kInfinity);
}

TEST(OptionTest, Bool)
{
    Stream stream;
    biyos::Options opts(stream.events);
    opts.set_bool(Option::Presolve, false);
    EXPECT_FALSE(opts.get_bool(Option::Presolve));
}

TEST(OptionTest, Int)
{
    Stream stream;
    biyos::Options opts(stream.events);
    opts.set_int(Option::Seed, 42);
    EXPECT_EQ(opts.get_int(Option::Seed), 42);
    opts.set_int(Option::Seed, (std::numeric_limits<int>::max)());
    EXPECT_EQ(opts.get_int(Option::Seed), (std::numeric_limits<int>::max)());
}

TEST(OptionTest, IntRange)
{
    Stream stream;
    biyos::Options opts(stream.events);
    opts.set_int(Option::Seed, 7);
    EXPECT_EQ(failure([&] { opts.set_int(Option::Seed, -1); }),
              "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(opts.get_int(Option::Seed), 7);
}

TEST(OptionTest, Double)
{
    Stream stream;
    biyos::Options opts(stream.events);
    opts.set_dbl(Option::TimeLimit, 2.5);
    EXPECT_DOUBLE_EQ(opts.get_dbl(Option::TimeLimit), 2.5);
    opts.set_dbl(Option::TimeLimit, 0.0);
    EXPECT_DOUBLE_EQ(opts.get_dbl(Option::TimeLimit), 0.0);
    opts.set_dbl(Option::TimeLimit, kInfinity);
    EXPECT_DOUBLE_EQ(opts.get_dbl(Option::TimeLimit), kInfinity);
}

TEST(OptionTest, DoubleRange)
{
    Stream stream;
    biyos::Options opts(stream.events);
    opts.set_dbl(Option::TimeLimit, 2.5);
    EXPECT_EQ(failure([&] { opts.set_dbl(Option::TimeLimit, -0.5); }),
              "invalid argument: option 'maxtime': -0.5 is outside [0, inf]");
    EXPECT_EQ(failure([&] { opts.set_dbl(Option::TimeLimit, kNan); }),
              "invalid argument: option 'maxtime': nan is outside [0, inf]");
    EXPECT_DOUBLE_EQ(opts.get_dbl(Option::TimeLimit), 2.5);
}

TEST(OptionTest, CpuLimit)
{
    Stream stream;
    biyos::Options opts(stream.events);
    opts.set_dbl(Option::CpuTimeLimit, 0.0);
    EXPECT_DOUBLE_EQ(opts.get_dbl(Option::CpuTimeLimit), 0.0);
    opts.set_dbl(Option::CpuTimeLimit, 2.5);
    EXPECT_EQ(failure([&] { opts.set_dbl(Option::CpuTimeLimit, -0.5); }),
              "invalid argument: option 'maxcputime': -0.5 is outside [0, inf]");
    EXPECT_EQ(failure([&] { opts.set_dbl(Option::CpuTimeLimit, kNan); }),
              "invalid argument: option 'maxcputime': nan is outside [0, inf]");
    EXPECT_DOUBLE_EQ(opts.get_dbl(Option::CpuTimeLimit), 2.5);
}

TEST(OptionTest, ToleranceRange)
{
    Stream stream;
    biyos::Options opts(stream.events);
    for (const Option opt :
         {Option::FeasibilityAbs, Option::FeasibilityRel, Option::IntegralityTol, Option::ZeroTol})
    {
        const std::string name(biyos::name_of(opt));
        opts.set_dbl(opt, 0.0);
        EXPECT_DOUBLE_EQ(opts.get_dbl(opt), 0.0);
        opts.set_dbl(opt, kInfinity);
        EXPECT_DOUBLE_EQ(opts.get_dbl(opt), kInfinity);
        opts.set_dbl(opt, 1e-3);
        EXPECT_EQ(failure([&] { opts.set_dbl(opt, -1e-9); }),
                  "invalid argument: option '" + name + "': -1e-09 is outside [0, inf]");
        EXPECT_EQ(failure([&] { opts.set_dbl(opt, kNan); }),
                  "invalid argument: option '" + name + "': nan is outside [0, inf]");
        EXPECT_DOUBLE_EQ(opts.get_dbl(opt), 1e-3);
    }
}

TEST(OptionTest, String)
{
    Stream stream;
    biyos::Options opts(stream.events);
    opts.set_str(Option::Reproducer, "replay.txt");
    EXPECT_EQ(opts.get_str(Option::Reproducer), "replay.txt");
}

TEST(OptionTest, Enum)
{
    Stream stream;
    biyos::Options opts(stream.events);
    opts.set_str(Option::CheckLevel, "full");
    EXPECT_EQ(opts.get_str(Option::CheckLevel), "full");
    opts.set_str(Option::CheckLevel, "none");
    EXPECT_EQ(opts.get_str(Option::CheckLevel), "none");
}

TEST(OptionTest, EnumValue)
{
    Stream stream;
    biyos::Options opts(stream.events);
    EXPECT_EQ(failure([&] { opts.set_str(Option::CheckLevel, "most"); }),
              "invalid argument: option 'check_lvl': 'most' is not one of none, cheap, full");
    EXPECT_EQ(opts.get_str(Option::CheckLevel), "cheap");
}

TEST(OptionTest, TypedEnum)
{
    using biyos::CheckLevel;
    Stream stream;
    biyos::Options opts(stream.events);
    EXPECT_EQ(opts.get_enum<CheckLevel>(Option::CheckLevel), CheckLevel::Cheap);
    opts.set_str(Option::CheckLevel, "full");
    EXPECT_EQ(opts.get_enum<CheckLevel>(Option::CheckLevel), CheckLevel::Full);
    opts.set_str(Option::CheckLevel, "none");
    EXPECT_EQ(opts.get_enum<CheckLevel>(Option::CheckLevel), CheckLevel::None);
    EXPECT_EQ(failure([&] { static_cast<void>(opts.get_enum<CheckLevel>(Option::Seed)); }),
              "invalid argument: option 'seed': is an int, not an enum");
}

TEST(OptionTest, WrongKind)
{
    Stream stream;
    biyos::Options opts(stream.events);
    EXPECT_EQ(failure([&] { opts.set_dbl(Option::Seed, 1.0); }),
              "invalid argument: option 'seed': is an int, not a double");
    EXPECT_EQ(failure([&] { opts.set_str(Option::Presolve, "yes"); }),
              "invalid argument: option 'presolve': is a bool, not a string");
    EXPECT_EQ(failure([&] { opts.set_int(Option::CheckLevel, 2); }),
              "invalid argument: option 'check_lvl': is an enum, not an int");
    EXPECT_EQ(failure([&] { static_cast<void>(opts.get_bool(Option::Reproducer)); }),
              "invalid argument: option 'repro': is a string, not a bool");
    EXPECT_EQ(failure([&] { static_cast<void>(opts.get_int(Option::TimeLimit)); }),
              "invalid argument: option 'maxtime': is a double, not an int");
}

TEST(OptionTest, Names)
{
    for (const Option opt : {Option::Seed,
                             Option::TimeLimit,
                             Option::CheckLevel,
                             Option::Presolve,
                             Option::Reproducer,
                             Option::FeasibilityAbs,
                             Option::FeasibilityRel,
                             Option::IntegralityTol,
                             Option::ZeroTol,
                             Option::CpuTimeLimit})
    {
        EXPECT_EQ(biyos::find_opt(biyos::name_of(opt)), opt);
        EXPECT_EQ(biyos::find_opt(static_cast<int>(opt)), opt);
    }
    EXPECT_EQ(biyos::name_of(Option::TimeLimit), "maxtime");
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

TEST(OptionTest, Versions)
{
    const biyos::Versions seed = biyos::versions_of(Option::Seed);
    EXPECT_EQ(seed.since, (Version{0, 1, 0}));
    EXPECT_FALSE(seed.deprecated);
    EXPECT_FALSE(seed.retired);
}

TEST(OptionTest, UnknownName)
{
    EXPECT_EQ(failure([] { biyos::find_opt("seeds"); }),
              "invalid argument: option 'seeds': unknown");
    EXPECT_EQ(failure([] { biyos::find_opt(""); }), "invalid argument: option '': unknown");
}

TEST(OptionTest, UnknownId)
{
    EXPECT_EQ(failure([] { biyos::find_opt(0); }), "invalid argument: option 0: unknown");
    EXPECT_EQ(failure([] { biyos::find_opt(11); }), "invalid argument: option 11: unknown");
    EXPECT_EQ(failure([] { biyos::find_opt(-3); }), "invalid argument: option -3: unknown");
}

TEST(OptionTest, MissingRow)
{
    Stream stream;
    biyos::Options opts(stream.events);
    const Option past = static_cast<Option>(11);
    const Option zero = static_cast<Option>(0);
    EXPECT_EQ(failure([&] { opts.set_int(past, 1); }),
              "internal error: option 11: has no table row");
    EXPECT_EQ(failure([&] { static_cast<void>(opts.get_str(zero)); }),
              "internal error: option 0: has no table row");
    EXPECT_EQ(failure([&] { static_cast<void>(biyos::name_of(past)); }),
              "internal error: option 11: has no table row");
    EXPECT_EQ(failure([&] { static_cast<void>(biyos::kind_of(past)); }),
              "internal error: option 11: has no table row");
    EXPECT_EQ(failure([&] { static_cast<void>(biyos::versions_of(past)); }),
              "internal error: option 11: has no table row");
}

TEST(OptionTest, Table)
{
    Stream stream;
    biyos::Options opts(kRows, stream.events);
    EXPECT_EQ(opts.get_int(Option{1}), 1);
    opts.set_int(Option{1}, 5);
    EXPECT_EQ(opts.get_int(Option{1}), 5);
    EXPECT_EQ(failure([&] { opts.set_int(Option{1}, -1); }),
              "invalid argument: option 'new': -1 is outside [0, 2147483647]");
    EXPECT_EQ(failure([&] { static_cast<void>(opts.get_int(Option{4})); }),
              "internal error: option 4: has no table row");
    EXPECT_TRUE(stream.heard.empty());
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

TEST(OptionTest, Retired)
{
    const std::string retired = "invalid argument: option 'gone': retired in 0.0.3, use 'new'";
    Stream stream;
    biyos::Options opts(kRows, stream.events);
    EXPECT_EQ(failure([&] { opts.set_int(Option{3}, 5); }), retired);
    EXPECT_EQ(failure([&] { opts.set_bool(Option{3}, true); }), retired);
    EXPECT_EQ(failure([&] { opts.set_dbl(Option{3}, 5.0); }), retired);
    EXPECT_EQ(failure([&] { opts.set_str(Option{3}, "5"); }), retired);
    EXPECT_EQ(opts.get_int(Option{3}), 1);
    EXPECT_TRUE(stream.heard.empty());
}

TEST(OptionTest, Deprecated)
{
    Stream stream;
    biyos::Options opts(kRows, stream.events);
    opts.set_int(Option{2}, 5);
    EXPECT_EQ(opts.get_int(Option{2}), 5);
    ASSERT_EQ(stream.heard.size(), 1U);
    const biyos::Event &evt = stream.heard.front();
    EXPECT_EQ(evt.kind, EventKind::Message);
    EXPECT_EQ(evt.level, EventLevel::Warning);
    EXPECT_EQ(evt.module, "core");
    EXPECT_EQ(evt.text, "option 'old': deprecated in 0.0.2, use 'new'");
    EXPECT_EQ(listed(evt.fields), "option=old deprecated=0.0.2 replacement=new");
    EXPECT_EQ(failure([&] { opts.set_int(Option{2}, -1); }),
              "invalid argument: option 'old': -1 is outside [0, 2147483647]");
    EXPECT_EQ(stream.heard.size(), 1U);
}
