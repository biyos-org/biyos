#include "biyos/biyos.h"
#include "core/env.hpp"
#include "core/events/event.hpp"
#include "core/options/options.hpp"
#include "model/model.hpp"
#include "model/session/session.hpp"

#include <gtest/gtest.h>

#include <span>

namespace
{

using biyos::Env;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::Option;
using biyos::Session;
using biyos::Variable;
using biyos::VariableKind;

constexpr int kSeed = 7;

void ignore(const biyos::Event &) {}

TEST(SessionTest, Fresh)
{
    Env env;
    const Session session(env);
    EXPECT_EQ(&session.env(), &env);
    EXPECT_EQ(session.model().n_variables(), 0);
    EXPECT_TRUE(session.settings().empty());
    EXPECT_FALSE(session.evaluating());
}

TEST(SessionTest, CopyModel)
{
    Env env;
    Session source(env);
    const Variable variable{.kind = VariableKind::Continuous, .lower = 1.0, .upper = 2.0};
    source.change().add_variables(std::span(&variable, 1));
    const Session copy(env, source);
    EXPECT_EQ(copy.model().n_variables(), 1);
}

TEST(SessionTest, CopySettings)
{
    Env env;
    Session source(env);
    source.settings().set_int(Option::Seed, kSeed, env.events());
    const Session copy(env, source);
    EXPECT_EQ(copy.lookup().get_int(Option::Seed), kSeed);
}

TEST(SessionTest, CopyHooks)
{
    Env env;
    Session source(env);
    source.add_hook(EventKind::Message, EventLevel::Debug, ignore);
    const Session copy(env, source);
    EXPECT_TRUE(source.hooks().wanted(EventKind::Message, EventLevel::Debug));
    EXPECT_FALSE(copy.hooks().wanted(EventKind::Message, EventLevel::Debug));
}

TEST(SessionTest, OtherEnv)
{
    Env env;
    Env other;
    const Session source(env);
    const Session copy(other, source);
    EXPECT_EQ(&copy.env(), &other);
    EXPECT_EQ(env.handles(), 1);
    EXPECT_EQ(other.handles(), 1);
}

}
