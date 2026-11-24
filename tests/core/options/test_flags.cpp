#include "core/error.hpp"
#include "core/events/events.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "core/options/settings.hpp"
#include "core/timing/clock.hpp"
#include "core/version.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using biyos::FlagPart;
using biyos::FlagsRule;
using biyos::Mask;
using biyos::Masks;
using biyos::Option;
using biyos::OptionSpec;
using biyos::PartDefault;
using biyos::PartState;
using biyos::Scope;
using biyos::Version;

constexpr Version kFirst{0, 0, 1};
constexpr Option kParts{1};
constexpr std::size_t kWidth = 64;
constexpr Mask kDual = Mask{1} << 0;
constexpr Mask kProbe = Mask{1} << 1;
constexpr Mask kScale = Mask{1} << 2;
constexpr std::string_view kMalformed = "internal error: option table: is not well formed";
constexpr std::string_view kSubject = "invalid argument: option 'parts': ";

constexpr std::array kList{
    FlagPart{
        .bit = kDual,
        .name = "dual",
        .default_value = PartDefault::On,
    },
    FlagPart{
        .bit = kProbe,
        .name = "probe",
        .default_value = PartDefault::Off,
    },
    FlagPart{
        .bit = kScale,
        .name = "scale",
        .default_value = PartDefault::Automatic,
    },
};

constexpr OptionSpec row(std::span<const FlagPart> parts)
{
    return OptionSpec{
        .id = kParts,
        .name = "parts",
        .scope = Scope::Model,
        .rule = FlagsRule{.parts = parts},
        .doc = "Parts.",
        .versions = {.since = kFirst},
    };
}

constexpr std::array kRows{row(kList)};

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

struct Wide
{
    explicit Wide(std::size_t count)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            names.push_back("p" + std::to_string(i));
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            const Mask bit = i < kWidth ? Mask{1} << i : 0;
            parts.push_back(FlagPart{
                .bit = bit,
                .name = names[i],
                .default_value = PartDefault::Automatic,
            });
        }
    }
    Wide(const Wide &) = delete;
    Wide &operator=(const Wide &) = delete;

    std::vector<std::string> names;
    std::vector<FlagPart> parts;
};

std::string built(std::span<const FlagPart> parts)
{
    const std::array rows{row(parts)};
    biyos::FakeClock clock;
    biyos::Events events{clock};
    return failure([&] { const biyos::Options options(rows, events); });
}

class FlagTest : public ::testing::Test
{
  protected:
    PartState state(Mask part) const { return lookup.part_state(kParts, part); }

    biyos::FakeClock clock;
    biyos::Events events{clock};
    biyos::Options options{kRows, events};
    biyos::Settings settings{kRows};
    biyos::Lookup lookup{settings, options};
};

}

TEST_F(FlagTest, Defaults)
{
    EXPECT_EQ(options.get_flags(kParts), Masks{});
    EXPECT_EQ(options.source_of(kParts), biyos::OptionSource::Default);
    EXPECT_EQ(state(kDual), PartState::Default);
    EXPECT_EQ(state(kProbe), PartState::Default);
    EXPECT_EQ(state(kScale), PartState::Default);
}

TEST_F(FlagTest, Force)
{
    options.set_flags(kParts, Masks{.on = kDual | kScale, .off = kProbe});
    EXPECT_EQ(options.get_flags(kParts), (Masks{.on = kDual | kScale, .off = kProbe}));
    EXPECT_EQ(state(kDual), PartState::On);
    EXPECT_EQ(state(kProbe), PartState::Off);
    EXPECT_EQ(state(kScale), PartState::On);
    options.set_flags(kParts, Masks{.on = 0, .off = kDual});
    EXPECT_EQ(state(kDual), PartState::Off);
    EXPECT_EQ(state(kProbe), PartState::Default);
    EXPECT_EQ(state(kScale), PartState::Default);
}

TEST_F(FlagTest, Refusal)
{
    const Masks set{.on = kDual, .off = kProbe};
    options.set_flags(kParts, set);
    const std::string outside = " outside its parts 0 (dual), 1 (probe), 2 (scale)";
    const Masks extra{.on = Mask{1} << 3, .off = 0};
    EXPECT_EQ(failure([&] { options.set_flags(kParts, extra); }),
              std::string(kSubject) + "sets bit 3" + outside);
    const Masks last{.on = 0, .off = Mask{1} << 63};
    EXPECT_EQ(failure([&] { options.set_flags(kParts, last); }),
              std::string(kSubject) + "sets bit 63" + outside);
    const Masks wide{.on = (Mask{1} << 63) | kDual, .off = Mask{1} << 5};
    EXPECT_EQ(failure([&] { options.set_flags(kParts, wide); }),
              std::string(kSubject) + "sets bits 5, 63" + outside);
    const Masks both{.on = kDual | kScale, .off = kDual | kProbe | kScale};
    EXPECT_EQ(failure([&] { options.set_flags(kParts, both); }),
              std::string(kSubject) + "sets bits 0, 2 both on and off");
    const Masks one{.on = kProbe, .off = kProbe};
    EXPECT_EQ(failure([&] { options.set_flags(kParts, one); }),
              std::string(kSubject) + "sets bit 1 both on and off");
    EXPECT_EQ(options.get_flags(kParts), set);
}

TEST_F(FlagTest, WrongKind)
{
    const std::string held = std::string(kSubject) + "is flags, not ";
    EXPECT_EQ(failure([&] { options.set_int(kParts, 1); }), held + "an int");
    EXPECT_EQ(failure([&] { options.set_bool(kParts, true); }), held + "a bool");
    EXPECT_EQ(failure([&] { options.set_double(kParts, 1.0); }), held + "a double");
    EXPECT_EQ(failure([&] { options.set_string(kParts, "dual"); }), held + "a string");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_int(kParts)); }), held + "an int");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_bool(kParts)); }), held + "a bool");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_double(kParts)); }), held + "a double");
    EXPECT_EQ(failure([&] { static_cast<void>(options.get_string(kParts)); }), held + "a string");
    EXPECT_EQ(failure([&] { static_cast<void>(lookup.get_int(kParts)); }), held + "an int");
    EXPECT_EQ(options.get_flags(kParts), Masks{});
}

TEST_F(FlagTest, OtherKinds)
{
    biyos::Options shipped(events);
    const biyos::Lookup found(shipped);
    const std::array<std::string_view, 5> held{"an int",
                                               "a double",
                                               "an enum",
                                               "a bool",
                                               "a string"};
    for (std::size_t i = 0; i < held.size(); ++i)
    {
        const auto option = static_cast<Option>(i + 1);
        const std::string name(biyos::name_of(option));
        const std::string wanted =
            "invalid argument: option '" + name + "': is " + std::string(held[i]) + ", not flags";
        EXPECT_EQ(failure([&] { shipped.set_flags(option, Masks{}); }), wanted);
        EXPECT_EQ(failure([&] { static_cast<void>(shipped.get_flags(option)); }), wanted);
        EXPECT_EQ(failure([&] { static_cast<void>(found.get_flags(option)); }), wanted);
        EXPECT_EQ(failure([&] { static_cast<void>(found.part_state(option, kDual)); }), wanted);
    }
}

TEST_F(FlagTest, Models)
{
    options.set_flags(kParts, Masks{.on = kDual, .off = kScale});
    settings.set_flags(kParts, Masks{.on = 0, .off = kProbe}, events);
    EXPECT_EQ(lookup.get_flags(kParts), (Masks{.on = 0, .off = kProbe}));
    EXPECT_EQ(state(kDual), PartState::Default);
    EXPECT_EQ(state(kProbe), PartState::Off);
    EXPECT_EQ(state(kScale), PartState::Default);
    EXPECT_EQ(
        failure([&] { settings.set_flags(kParts, Masks{.on = kDual, .off = kDual}, events); }),
        std::string(kSubject) + "sets bit 0 both on and off");
    EXPECT_EQ(options.get_flags(kParts), (Masks{.on = kDual, .off = kScale}));
}

TEST_F(FlagTest, Unlisted)
{
    const std::string_view unlisted = "internal error: option 'parts': has no part ";
    EXPECT_EQ(failure([&] { static_cast<void>(state(Mask{1} << 3)); }),
              std::string(unlisted) + "8");
    EXPECT_EQ(failure([&] { static_cast<void>(state(kDual | kProbe)); }),
              std::string(unlisted) + "3");
    EXPECT_EQ(failure([&] { static_cast<void>(state(0)); }), std::string(unlisted) + "0");
}

TEST_F(FlagTest, BadTable)
{
    EXPECT_EQ(built(kList), "no error");
    EXPECT_EQ(built({}), kMalformed);
    const std::array gapped{kList[0], kList[2]};
    EXPECT_EQ(built(gapped), kMalformed);
    const std::array swapped{kList[1], kList[0]};
    EXPECT_EQ(built(swapped), kMalformed);
    std::array twins = kList;
    twins[2].name = "dual";
    EXPECT_EQ(built(twins), kMalformed);
    const Wide full(kWidth);
    EXPECT_EQ(built(full.parts), "no error");
    const Wide over(kWidth + 1);
    EXPECT_EQ(built(over.parts), kMalformed);
}
