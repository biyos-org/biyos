#include "sample.hpp"

#include "core/env/env.hpp"
#include "core/env/error.hpp"
#include "core/env/version.hpp"
#include "core/events/events.hpp"
#include "core/numbers.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "core/options/settings.hpp"
#include "core/options/table.hpp"
#include "core/options/text.hpp"
#include "core/timing/clock.hpp"
#include "io/reproducer/reproducer.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using biyos::BoolRule;
using biyos::Checking;
using biyos::difference;
using biyos::dotted;
using biyos::DoubleRule;
using biyos::EnumRule;
using biyos::EnumValue;
using biyos::Env;
using biyos::Error;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::FlagPart;
using biyos::FlagsRule;
using biyos::IntRule;
using biyos::Lookup;
using biyos::Masks;
using biyos::Model;
using biyos::Option;
using biyos::option_table;
using biyos::OptionSource;
using biyos::OptionSpec;
using biyos::PartDefault;
using biyos::Real;
using biyos::Scope;
using biyos::Settings;
using biyos::StringRule;
using biyos::SystemClock;
using biyos::Version;
using biyos::version;
using biyos::test::built;
using biyos::test::sample;

namespace
{

constexpr Version kFirst{0, 0, 1};
constexpr Version kSecond{0, 0, 2};
constexpr Version kThird{0, 0, 3};

constexpr std::array kLevels{
    EnumValue{.number = 1, .name = "low"},
    EnumValue{.number = 2, .name = "high"},
};

constexpr std::array kParts{
    FlagPart{.bit = 1, .name = "left", .default_value = PartDefault::On},
    FlagPart{.bit = 2, .name = "right", .default_value = PartDefault::Off},
};

constexpr std::array kRows{
    OptionSpec{
        .id = Option{1},
        .name = "flag",
        .scope = Scope::Model,
        .rule = BoolRule{.default_value = true},
        .doc = "Bool.",
        .versions = {.since = kFirst},
    },
    OptionSpec{
        .id = Option{2},
        .name = "count",
        .scope = Scope::Model,
        .rule = IntRule{.default_value = 1, .lower = 0},
        .doc = "Int.",
        .versions = {.since = kFirst},
    },
    OptionSpec{
        .id = Option{3},
        .name = "ratio",
        .scope = Scope::Model,
        .rule = DoubleRule{.default_value = 0.5},
        .doc = "Double.",
        .versions = {.since = kFirst},
    },
    OptionSpec{
        .id = Option{4},
        .name = "label",
        .scope = Scope::Model,
        .rule = StringRule{.default_value = "plain"},
        .doc = "String.",
        .versions = {.since = kFirst},
    },
    OptionSpec{
        .id = Option{5},
        .name = "level",
        .scope = Scope::Model,
        .rule = EnumRule{.default_value = 1, .values = kLevels},
        .doc = "Enum.",
        .versions = {.since = kFirst},
    },
    OptionSpec{
        .id = Option{6},
        .name = "sides",
        .scope = Scope::Model,
        .rule = FlagsRule{.parts = kParts},
        .doc = "Flags.",
        .versions = {.since = kFirst},
    },
    OptionSpec{
        .id = Option{7},
        .name = "width",
        .scope = Scope::Environment,
        .rule = IntRule{.default_value = 1, .lower = 1},
        .doc = "Environment.",
        .versions = {.since = kFirst},
    },
    OptionSpec{
        .id = Option{8},
        .name = "gone",
        .scope = Scope::Model,
        .rule = IntRule{.default_value = 1},
        .doc = "Retired.",
        .versions = {.since = kFirst, .deprecated = kSecond, .retired = kThird},
        .replacement = Option{2},
    },
    OptionSpec{
        .id = Option{9},
        .name = "old",
        .scope = Scope::Model,
        .rule = IntRule{.default_value = 1},
        .doc = "Deprecated.",
        .versions = {.since = kFirst, .deprecated = kSecond},
        .replacement = Option{2},
    },
};

constexpr Option kFlag{1};
constexpr Option kCount{2};
constexpr Option kRatio{3};
constexpr Option kLabel{4};
constexpr Option kLevel{5};
constexpr Option kSides{6};
constexpr Option kWidth{7};

struct Listener
{
    Listener() : Listener(option_table()) {}
    explicit Listener(std::span<const OptionSpec> table)
        : env(std::make_unique<SystemClock>(), table), settings(table)
    {
        const auto record = [this](const Event &event) { warnings.push_back(event.text); };
        env.events().add_hook(EventKind::Message, EventLevel::Warning, record);
    }
    Listener(const Listener &) = delete;
    Listener &operator=(const Listener &) = delete;

    Env env;
    Settings settings;
    std::vector<std::string> warnings;
};

std::string written(const Model &model, const Lookup &lookup)
{
    std::ostringstream output;
    print_reproducer(model, lookup, output);
    return output.str();
}

Model replayed(Listener &listener, std::string_view text)
{
    std::istringstream input{std::string(text)};
    return read_reproducer(listener.env, listener.settings, input, "t.repro");
}

std::string fault(Listener &listener, std::string_view text)
{
    try
    {
        replayed(listener, text);
    }
    catch (const Error &error)
    {
        return error.what();
    }
    return "";
}

std::string bad_line(int line, std::string_view message)
{
    return "bad file: file 't.repro' line " + std::to_string(line) + ": " + std::string(message);
}

std::string warned(int line, std::string_view message)
{
    return "file 't.repro' line " + std::to_string(line) + ": " + std::string(message);
}

std::string current()
{
    return "VERSION " + dotted(version()) + "\n";
}

std::string custom(std::string_view options)
{
    return "REPRO 1\n" + current() + std::string(options) + "BIYOS 1\nEND\n";
}

Real carried(Real value)
{
    Listener source(kRows);
    source.env.options().set_double(kRatio, value);
    Listener target(kRows);
    replayed(target, written(Model(), Lookup(source.env.options())));
    return target.env.options().get_double(kRatio);
}

std::string carried(std::string_view value)
{
    Listener source(kRows);
    source.env.options().set_string(kLabel, value);
    Listener target(kRows);
    replayed(target, written(Model(), Lookup(source.env.options())));
    return std::string(target.env.options().get_string(kLabel));
}

}

TEST(ReproducerTest, RoundTrip)
{
    Listener source;
    source.env.options().set_int(Option::Seed, 7);
    source.env.options().set_double(Option::TimeLimit, 12.5);
    source.env.options().set_int(Option::Checking, static_cast<int>(Checking::Full));
    source.env.options().set_string(Option::Reproducer, "run 1.repro");
    Settings settings;
    settings.set_bool(Option::Presolve, false, source.env.stream());
    settings.set_double(Option::FeasAbs, 1e-8, source.env.stream());
    const Model model = built(sample());
    const std::string text = written(model, Lookup(settings, source.env.options()));
    EXPECT_NE(text.find("\npresolve model false\n"), std::string::npos);
    EXPECT_NE(text.find("\nseed environment 7\n"), std::string::npos);
    EXPECT_NE(text.find("\nthreads default\n"), std::string::npos);

    Listener target;
    target.env.options().set_double(Option::ZeroTol, 1e-12);
    const Model back = replayed(target, text);
    EXPECT_EQ(difference(model, back), "");
    EXPECT_TRUE(target.warnings.empty());
    EXPECT_EQ(target.env.options().get_int(Option::Seed), 7);
    EXPECT_EQ(target.env.options().source_of(Option::ZeroTol), OptionSource::Default);
    EXPECT_EQ(target.env.options().source_of(Option::Presolve), OptionSource::Default);
    const Lookup lookup(target.settings, target.env.options());
    EXPECT_FALSE(lookup.get_bool(Option::Presolve));
    EXPECT_EQ(lookup.find(Option::FeasAbs).source, OptionSource::Model);
    EXPECT_EQ(written(back, lookup), text);
}

TEST(ReproducerTest, Kinds)
{
    Listener source(kRows);
    source.env.options().set_bool(kFlag, false);
    source.env.options().set_int(kCount, 3);
    source.env.options().set_double(kRatio, -biyos::kInfinity);
    source.env.options().set_string(kLabel, "two words");
    source.env.options().set_int(kLevel, 2);
    source.env.options().set_flags(kSides, Masks{.on = 2, .off = 1});
    source.env.options().set_int(kWidth, 4);
    const std::string text = written(Model(), Lookup(source.env.options()));
    EXPECT_EQ(text,
              "REPRO 1\n" + current() +
                  "OPTIONS 8\nflag environment false\ncount environment 3\n"
                  "ratio environment -inf\nlabel environment two\\20words\n"
                  "level environment high\nsides environment -left +right\n"
                  "width environment 4\nold default\nBIYOS 1\nEND\n");

    Listener target(kRows);
    replayed(target, text);
    EXPECT_EQ(written(Model(), Lookup(target.env.options())), text);
}

TEST(ReproducerTest, EmptyValues)
{
    Listener listener(kRows);
    listener.env.options().set_string(kLabel, "");
    listener.env.options().set_flags(kSides, Masks{});
    const std::string text = written(Model(), Lookup(listener.env.options()));
    EXPECT_NE(text.find("\nlabel environment\nlevel default\nsides environment\n"),
              std::string::npos);
    listener.env.options().drop(kLabel);
    listener.env.options().drop(kSides);
    replayed(listener, text);
    EXPECT_EQ(listener.env.options().get_string(kLabel), "");
    EXPECT_EQ(listener.env.options().source_of(kLabel), OptionSource::Environment);
    EXPECT_EQ(listener.env.options().get_flags(kSides), Masks{});
    EXPECT_EQ(listener.env.options().source_of(kSides), OptionSource::Environment);
}

TEST(ReproducerTest, Absent)
{
    Listener listener(kRows);
    listener.env.options().set_int(kCount, 5);
    listener.env.options().set_int(kWidth, 3);
    replayed(listener, custom("OPTIONS 1\nratio environment 2\n"));
    EXPECT_DOUBLE_EQ(listener.env.options().get_double(kRatio), 2.0);
    EXPECT_EQ(listener.env.options().source_of(kCount), OptionSource::Default);
    EXPECT_EQ(listener.env.options().get_int(kCount), 1);
    EXPECT_EQ(listener.env.options().get_int(kWidth), 1);
}

TEST(ReproducerTest, Sources)
{
    Listener source(kRows);
    source.env.options().set_int(kCount, 3);
    source.settings.set_int(kCount, 4, source.env.stream());
    source.env.options().set_int(kWidth, 2);
    const std::string text = written(Model(), Lookup(source.settings, source.env.options()));
    EXPECT_NE(text.find("\ncount model 4\n"), std::string::npos);
    EXPECT_NE(text.find("\nwidth environment 2\n"), std::string::npos);
    EXPECT_NE(text.find("\nflag default\n"), std::string::npos);

    Listener target(kRows);
    target.env.options().set_bool(kFlag, false);
    replayed(target, text);
    EXPECT_EQ(target.env.options().source_of(kFlag), OptionSource::Default);
    EXPECT_EQ(target.env.options().source_of(kCount), OptionSource::Default);
    EXPECT_EQ(target.env.options().get_int(kWidth), 2);
    const Lookup lookup(target.settings, target.env.options());
    EXPECT_EQ(lookup.find(kCount).source, OptionSource::Model);
    EXPECT_EQ(lookup.get_int(kCount), 4);
    EXPECT_EQ(written(Model(), lookup), text);
}

TEST(ReproducerTest, Deprecated)
{
    Listener listener(kRows);
    replayed(listener, custom("OPTIONS 1\nold default\n"));
    EXPECT_TRUE(listener.warnings.empty());
    replayed(listener, custom("OPTIONS 1\nold model 2\n"));
    EXPECT_EQ(listener.warnings,
              std::vector<std::string>{"option 'old': deprecated in 0.0.2, use 'count'"});
}

TEST(ReproducerTest, Doubles)
{
    const std::array values{
        0.1 + 0.2,
        5e-324,
        -1e-310,
        -0.0,
        (std::numeric_limits<Real>::max)(),
        -(std::numeric_limits<Real>::max)(),
    };
    for (const Real value : values)
    {
        EXPECT_EQ(std::bit_cast<std::uint64_t>(carried(value)),
                  std::bit_cast<std::uint64_t>(value));
    }
}

TEST(ReproducerTest, Strings)
{
    const std::array<std::string_view, 15> values{
        "",
        " ",
        "two  blanks",
        "\"quoted\"",
        "a\nb",
        "a\rb",
        "a\tb",
        "back\\slash",
        "#hash",
        "\\20",
        "\xc3\xa9t\xc3\xa9",
        " lead",
        "trail ",
        "+plus",
        "'single'",
    };
    for (const std::string_view value : values)
    {
        EXPECT_EQ(carried(value), value);
    }
}

TEST(ReproducerTest, Version)
{
    Listener listener(kRows);
    replayed(listener, "REPRO 1\nVERSION 0.0.1\nOPTIONS 0\nBIYOS 1\nEND\n");
    ASSERT_EQ(listener.warnings.size(), 1U);
    EXPECT_EQ(listener.warnings.front(),
              warned(2, "the file is from biyos 0.0.1, read by " + dotted(version())));
}

TEST(ReproducerTest, Unknown)
{
    Listener listener(kRows);
    replayed(listener, custom("OPTIONS 2\nnosuch 3 4\ncount environment 2\n"));
    ASSERT_EQ(listener.warnings.size(), 1U);
    EXPECT_EQ(listener.warnings.front(),
              warned(4, "option 'nosuch' is skipped: unknown to biyos " + dotted(version())));
    EXPECT_EQ(listener.env.options().get_int(kCount), 2);
}

TEST(ReproducerTest, Retired)
{
    Listener listener(kRows);
    replayed(listener, custom("OPTIONS 2\ngone environment 2\ncount environment 4\n"));
    ASSERT_EQ(listener.warnings.size(), 1U);
    EXPECT_EQ(listener.warnings.front(),
              warned(4, "option 'gone' is skipped: retired in 0.0.3, use 'count'"));
    EXPECT_EQ(listener.env.options().get_int(kCount), 4);
}

TEST(ReproducerTest, Faults)
{
    Listener listener(kRows);
    EXPECT_EQ(fault(listener, ""), "bad file: file 't.repro': is empty");
    EXPECT_EQ(fault(listener, "BIYOS 1\nEND\n"), bad_line(1, "the file does not start with REPRO"));
    EXPECT_EQ(fault(listener, "REPRO 2\n"),
              "unsupported: file 't.repro' line 1: version 2 is newer than 1");
    EXPECT_EQ(fault(listener, "REPRO 1\n"), bad_line(1, "the file ends before VERSION"));
    EXPECT_EQ(fault(listener, "REPRO 1\nVERSION 1.2\n"), bad_line(2, "'1.2' is not a version"));
    EXPECT_EQ(fault(listener, "REPRO 1\nVERSION 0.100.0\n"),
              bad_line(2, "'0.100.0' is not a version"));
    EXPECT_EQ(fault(listener, "REPRO 1\n" + current() + "COUNT 1\n"),
              bad_line(3, "the line does not start with OPTIONS"));
    EXPECT_EQ(fault(listener, custom("OPTIONS x\n")), bad_line(3, "'x' is not a count"));
    EXPECT_EQ(fault(listener, "REPRO 1\n" + current() + "OPTIONS 2\ncount environment 1\n"),
              bad_line(4, "the file ends inside OPTIONS"));
}

TEST(ReproducerTest, BadValues)
{
    Listener listener(kRows);
    listener.env.options().set_int(kCount, 6);
    const auto bad = [&](std::string_view line) { return fault(listener, custom(line)); };
    EXPECT_EQ(bad("OPTIONS 1\nflag environment yes\n"), bad_line(4, "'yes' is not true or false"));
    EXPECT_EQ(bad("OPTIONS 1\ncount environment -1\n"),
              bad_line(4, "option 'count': -1 is outside [0, 2147483647]"));
    EXPECT_EQ(bad("OPTIONS 1\ncount environment 1 2\n"),
              bad_line(4, "the line holds too many fields"));
    EXPECT_EQ(bad("OPTIONS 1\ncount\n"), bad_line(4, "the line ends early"));
    EXPECT_EQ(bad("OPTIONS 1\ncount environment\n"), bad_line(4, "the line ends early"));
    EXPECT_EQ(bad("OPTIONS 1\ncount fixed 2\n"),
              bad_line(4, "'fixed' is not model, environment or default"));
    EXPECT_EQ(bad("OPTIONS 1\ncount default 2\n"), bad_line(4, "the line holds too many fields"));
    EXPECT_EQ(bad("OPTIONS 1\nwidth model 2\n"),
              bad_line(4, "option 'width' has environment scope"));
    EXPECT_EQ(bad("OPTIONS 2\ncount default\ncount default\n"),
              bad_line(5, "option 'count' is set twice"));
    EXPECT_EQ(bad("OPTIONS 1\nlevel environment top\n"),
              bad_line(4, "'top' is not one of its values"));
    EXPECT_EQ(bad("OPTIONS 1\nsides environment left\n"),
              bad_line(4, "'left' does not start with + or -"));
    EXPECT_EQ(bad("OPTIONS 1\nsides environment +up\n"),
              bad_line(4, "'up' is not one of its parts"));
    EXPECT_EQ(bad("OPTIONS 1\nsides environment +left -left\n"),
              bad_line(4, "option 'sides': sets bit 0 both on and off"));
    EXPECT_EQ(bad("OPTIONS 2\ncount environment 1\ncount model 2\n"),
              bad_line(5, "option 'count' is set twice"));
    EXPECT_EQ(fault(listener, "REPRO 1\n" + current() + "OPTIONS 1\ncount environment 2\nEND\n"),
              bad_line(5, "the model does not start with BIYOS"));
    EXPECT_EQ(listener.env.options().get_int(kCount), 6);
}
