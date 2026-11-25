#include "core/env.hpp"
#include "core/events/event.hpp"
#include "core/events/stream.hpp"
#include "core/timing/timer.hpp"
#include "model/session/session.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

using biyos::EventKind;
using biyos::EventLevel;
using biyos::Session;
using Models = std::vector<const Session *>;
using Texts = std::vector<std::string>;

struct Heard
{
    Texts edges;
    Models models;
};

void note(Heard &heard, const biyos::Event &event)
{
    heard.edges.push_back(event.text + " " + event.fields[0].value);
    heard.models.push_back(event.model);
}

biyos::Hook listener(Heard &heard)
{
    return [&heard](const biyos::Event &event) { note(heard, event); };
}

}

TEST(TimerTest, ModelSpan)
{
    biyos::Env env;
    Session session(env);
    Session other(env);
    Heard model;
    Heard stray;
    Heard shared;
    session.add_hook(EventKind::Span, EventLevel::Debug, listener(model));
    other.add_hook(EventKind::Span, EventLevel::Debug, listener(stray));
    env.events().add_hook(EventKind::Span, EventLevel::Debug, listener(shared));
    {
        const biyos::EntryTimer timer(session.stream(), "io", "write");
    }
    const Texts edges{"write begin", "write end"};
    EXPECT_EQ(model.edges, edges);
    EXPECT_EQ(shared.edges, edges);
    EXPECT_EQ(model.models, Models(2, &session));
    EXPECT_EQ(shared.models, Models(2, &session));
    EXPECT_TRUE(stray.edges.empty());
}
