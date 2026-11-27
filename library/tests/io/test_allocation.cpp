#include "allocations.hpp"
#include "core/env/env.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "core/options/settings.hpp"
#include "core/options/table.hpp"
#include "core/timing/clock.hpp"
#include "io/reproducer/reproducer.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

namespace
{

using biyos::Env;
using biyos::Lookup;
using biyos::Model;
using biyos::Option;
using biyos::option_table;
using biyos::OptionSource;
using biyos::Settings;
using biyos::SystemClock;
using biyos::test::fail_after;
using biyos::test::stop_failing;

constexpr std::size_t kAttempts = 1000;
constexpr std::string_view kBefore = "a file name too long to sit inside the string";
constexpr std::string_view kAfter = "another file name too long to sit inside the string";

struct PendingFailure
{
    explicit PendingFailure(std::size_t successes) noexcept { fail_after(successes); }
    PendingFailure(const PendingFailure &) = delete;
    PendingFailure &operator=(const PendingFailure &) = delete;
    ~PendingFailure() { stop_failing(); }
};

std::string reproducer()
{
    Env env(std::make_unique<SystemClock>(), option_table());
    env.options().set_int(Option::Seed, 7);
    env.options().set_string(Option::Reproducer, kAfter);
    Settings settings;
    settings.set_bool(Option::Presolve, false, env.stream());
    std::ostringstream output;
    print_reproducer(Model(), Lookup(settings, env.options()), output);
    return output.str();
}

bool runs_out(Env &env, Settings &settings, const std::string &text, std::size_t successes)
{
    std::istringstream input(text);
    const PendingFailure failure(successes);
    try
    {
        read_reproducer(env, settings, input, "t.repro");
    }
    catch (const std::exception &)
    {
        return true;
    }
    return false;
}

}

TEST(AllocationTest, Reproducer)
{
    const std::string text = reproducer();
    for (std::size_t successes = 0; successes < kAttempts; ++successes)
    {
        Env env(std::make_unique<SystemClock>(), option_table());
        env.options().set_int(Option::Seed, 3);
        env.options().set_string(Option::Reproducer, kBefore);
        Settings settings;
        if (!runs_out(env, settings, text, successes))
        {
            EXPECT_GT(successes, 0U);
            EXPECT_EQ(env.options().get_int(Option::Seed), 7);
            EXPECT_EQ(env.options().get_string(Option::Reproducer), kAfter);
            EXPECT_NE(settings.find(Option::Presolve), nullptr);
            return;
        }
        EXPECT_EQ(env.options().get_int(Option::Seed), 3);
        EXPECT_EQ(env.options().get_string(Option::Reproducer), kBefore);
        EXPECT_EQ(env.options().source_of(Option::TimeLimit), OptionSource::Default);
    }
    ADD_FAILURE() << "the call never completed";
}
