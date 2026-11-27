#include "core/env/env.hpp"
#include "core/env/version.hpp"
#include "core/events/events.hpp"
#include "core/events/stream.hpp"
#include "core/options/options.hpp"
#include "core/options/settings.hpp"
#include "core/options/table.hpp"
#include "model/session/session.hpp"

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace
{

using biyos::Env;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::IntRule;
using biyos::Option;
using biyos::Options;
using biyos::OptionSpec;
using biyos::Scope;
using biyos::Session;
using biyos::Settings;
using biyos::Version;

constexpr Version kFirst{0, 0, 1};
constexpr Version kSecond{0, 0, 2};
constexpr Option kNew{1};
constexpr Option kOld{2};
constexpr auto kWarning = "option 'old': deprecated in 0.0.2, use 'new'";

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
};

class WarningTest : public ::testing::Test
{
  protected:
    WarningTest()
    {
        const auto record = [this](const Event &event) { heard.push_back(event); };
        env.events().add_hook(EventKind::Message, EventLevel::Debug, record);
    }

    Env env;
    Session session{env};
    std::vector<Event> heard;
    Options options{kRows, env};
    Settings settings{kRows};
};

}

TEST_F(WarningTest, Deprecated)
{
    settings.set_int(kOld, 5, session.stream());
    options.set_int(kOld, 6);
    ASSERT_EQ(heard.size(), 2U);
    EXPECT_EQ(heard[0].model, &session);
    EXPECT_EQ(heard[1].model, nullptr);
    EXPECT_EQ(heard[0].text, kWarning);
}

TEST_F(WarningTest, ModelHooks)
{
    std::vector<Event> local;
    const auto keep = [&local](const Event &event) { local.push_back(event); };
    session.add_hook(EventKind::Message, EventLevel::Warning, keep);
    settings.set_int(kOld, 5, session.stream());
    settings.set_int(kNew, 6, session.stream());
    options.set_int(kOld, 7);
    ASSERT_EQ(local.size(), 1U);
    EXPECT_EQ(local[0].model, &session);
    EXPECT_EQ(local[0].text, kWarning);
    EXPECT_EQ(heard.size(), 2U);
}
