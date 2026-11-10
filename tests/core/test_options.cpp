#include "core/error.hpp"
#include "core/options.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <limits>
#include <string>

namespace
{

using biyos::Option;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

std::string failure(const std::function<void()> &body)
{
    try
    {
        body();
    }
    catch (const biyos::Error &error)
    {
        EXPECT_EQ(error.kind(), biyos::ErrorKind::InvalidArgument);
        return error.what();
    }
    return "no error";
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
    for (const Option option : {Option::Seed, Option::TimeLimit, Option::CheckLevel,
                                Option::Presolve, Option::Reproducer})
    {
        EXPECT_EQ(biyos::find_option(biyos::name_of(option)), option);
        EXPECT_EQ(biyos::find_option(static_cast<int>(option)), option);
    }
    EXPECT_EQ(biyos::name_of(Option::TimeLimit), "time_limit");
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
    EXPECT_EQ(failure([] { biyos::find_option(6); }), "invalid argument: option 6: unknown");
    EXPECT_EQ(failure([] { biyos::find_option(-3); }), "invalid argument: option -3: unknown");
}
