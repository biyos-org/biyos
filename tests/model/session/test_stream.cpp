#include "biyos/biyos.h"
#include "core/env.hpp"
#include "core/error.hpp"
#include "core/events/event.hpp"
#include "core/events/stream.hpp"
#include "delivery.hpp"
#include "model/session/session.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

using biyos::ErrorKind;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::Session;
using biyos::test::quiet;
using Texts = std::vector<std::string>;
using Models = std::vector<const Session *>;

void env_scope(biyos::Env &env)
{
    const biyos::ErrorScope scope(env);
    env.stream().record(ErrorKind::InvalidArgument, "env");
    EXPECT_EQ(env.last_error().model, nullptr);
}

class StreamTest : public ::testing::Test
{
  protected:
    biyos::Hook recorder(const std::string &name)
    {
        return [this, name](const biyos::Event &event) { note(name, event); };
    }

    void note(const std::string &name, const biyos::Event &event)
    {
        texts.push_back(name);
        models.push_back(event.model);
    }

    void listen()
    {
        env.events().add_hook(EventKind::Message, EventLevel::Debug, recorder("env"));
        session.add_hook(EventKind::Message, EventLevel::Debug, recorder("model"));
    }

    biyos::Env env;
    Session session{env};
    Session other{env};
    Texts texts;
    Models models;
};

}

TEST_F(StreamTest, ModelRoute)
{
    listen();
    session.stream().emit(EventKind::Message, EventLevel::Info, quiet);
    EXPECT_EQ(texts, (Texts{"model", "env"}));
    EXPECT_EQ(models, (Models{&session, &session}));
}

TEST_F(StreamTest, EnvRoute)
{
    listen();
    env.stream().emit(EventKind::Message, EventLevel::Info, quiet);
    EXPECT_EQ(texts, Texts{"env"});
    EXPECT_EQ(models, Models{nullptr});
}

TEST_F(StreamTest, OtherModel)
{
    listen();
    other.stream().emit(EventKind::Message, EventLevel::Info, quiet);
    EXPECT_EQ(texts, Texts{"env"});
    EXPECT_EQ(models, Models{&other});
}

TEST_F(StreamTest, Wanted)
{
    session.add_hook(EventKind::Message, EventLevel::Warning, recorder("model"));
    EXPECT_TRUE(session.stream().wanted(EventKind::Message, EventLevel::Warning));
    EXPECT_FALSE(env.stream().wanted(EventKind::Message, EventLevel::Warning));
    EXPECT_FALSE(session.stream().wanted(EventKind::Message, EventLevel::Info));
}

TEST_F(StreamTest, Owner)
{
    EXPECT_EQ(session.stream().session(), &session);
    EXPECT_EQ(env.stream().session(), nullptr);
}

TEST_F(StreamTest, ModelRecord)
{
    session.stream().record(ErrorKind::InvalidArgument, "bad");
    EXPECT_EQ(env.last_error().status, BIYOS_ERR_BADARG);
    EXPECT_EQ(env.last_error().model, &session);
    EXPECT_STREQ(env.last_error().text, "bad");
}

TEST_F(StreamTest, EnvRecord)
{
    session.stream().record(ErrorKind::InvalidArgument, "bad");
    env.stream().record(ErrorKind::Internal, "broken");
    EXPECT_EQ(env.last_error().status, BIYOS_ERR_INTERNAL);
    EXPECT_EQ(env.last_error().model, nullptr);
}

TEST_F(StreamTest, ScopedRecord)
{
    session.stream().record(ErrorKind::InvalidArgument, "model");
    biyos::test::in_delivery(env, env_scope);
    EXPECT_EQ(env.last_error().model, &session);
    EXPECT_STREQ(env.last_error().text, "model");
}
