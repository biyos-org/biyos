#include "biyos/biyos.h"
#include "capi/event.hpp"
#include "core/events.hpp"

#include <gtest/gtest.h>

#include <string>

namespace
{

class Env
{
  public:
    Env() { EXPECT_EQ(biyos_env_create(&ptr_), BIYOS_OK); }
    ~Env() { biyos_env_free(ptr_); }
    Env(const Env &) = delete;
    Env &operator=(const Env &) = delete;

    biyos_env *get() const { return ptr_; }
    std::string message() const { return biyos_last_error_message(ptr_); }

  private:
    biyos_env *ptr_ = nullptr;
};

struct Inbox
{
    int calls = 0;
    int kind = 0;
    int level = 0;
    std::string module;
    std::string text;
};

struct Ticket
{
    biyos_env *env = nullptr;
    int id = 0;
    int calls = 0;
    biyos_status status = BIYOS_INTERNAL_ERROR;
};

struct Probe
{
    biyos_env *env = nullptr;
    int calls = 0;
    biyos_status status = BIYOS_OK;
    std::string message;
};

void receive(const biyos_event *event, void *user_data)
{
    Inbox *inbox = static_cast<Inbox *>(user_data);
    ++inbox->calls;
    inbox->kind = biyos_event_kind(event);
    inbox->level = biyos_event_level(event);
    inbox->module = biyos_event_module(event);
    inbox->text = biyos_event_text(event);
}

void leave(const biyos_event *, void *user_data)
{
    Ticket *ticket = static_cast<Ticket *>(user_data);
    ++ticket->calls;
    ticket->status = biyos_unsubscribe(ticket->env, ticket->id);
}

void unsubscribe_unknown(const biyos_event *, void *user_data)
{
    Probe *probe = static_cast<Probe *>(user_data);
    ++probe->calls;
    probe->status = biyos_unsubscribe(probe->env, 999);
    probe->message = biyos_last_error_message(probe->env);
}

biyos_status listen(biyos_env *env, biyos_subscriber subscriber, void *user_data, int lowest,
                    int *id)
{
    return biyos_subscribe(env, subscriber, user_data, BIYOS_EVENT_MESSAGE, lowest, id);
}

void fail(const Env &env)
{
    EXPECT_EQ(biyos_set_int_option(env.get(), BIYOS_OPT_SEED, -1), BIYOS_INVALID_ARGUMENT);
}

}

TEST(CApiTest, Subscribe)
{
    const Env env;
    Inbox inbox;
    int id = 0;
    ASSERT_EQ(listen(env.get(), receive, &inbox, BIYOS_LEVEL_ERROR, &id), BIYOS_OK);
    EXPECT_EQ(id, 1);
    fail(env);
    EXPECT_EQ(inbox.calls, 1);
    EXPECT_EQ(inbox.kind, BIYOS_EVENT_MESSAGE);
    EXPECT_EQ(inbox.level, BIYOS_LEVEL_ERROR);
    EXPECT_EQ(inbox.module, "capi");
    EXPECT_EQ(inbox.text, "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(inbox.text, env.message());
}

TEST(CApiTest, Unsubscribe)
{
    const Env env;
    Inbox inbox;
    int id = 0;
    ASSERT_EQ(listen(env.get(), receive, &inbox, BIYOS_LEVEL_DEBUG, &id), BIYOS_OK);
    EXPECT_EQ(biyos_unsubscribe(env.get(), id), BIYOS_OK);
    fail(env);
    EXPECT_EQ(inbox.calls, 0);
    EXPECT_EQ(biyos_unsubscribe(env.get(), id), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: subscription 1: unknown");
}

TEST(CApiTest, OneShot)
{
    const Env env;
    Ticket ticket;
    ticket.env = env.get();
    ASSERT_EQ(listen(env.get(), leave, &ticket, BIYOS_LEVEL_ERROR, &ticket.id), BIYOS_OK);
    fail(env);
    fail(env);
    EXPECT_EQ(ticket.calls, 1);
    EXPECT_EQ(ticket.status, BIYOS_OK);
}

TEST(CApiTest, NestedFailure)
{
    const Env env;
    Probe probe;
    probe.env = env.get();
    int id = 0;
    ASSERT_EQ(listen(env.get(), unsubscribe_unknown, &probe, BIYOS_LEVEL_ERROR, &id), BIYOS_OK);
    fail(env);
    EXPECT_EQ(probe.calls, 1);
    EXPECT_EQ(probe.status, BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(probe.message, "invalid argument: subscription 999: unknown");
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
}

TEST(CApiTest, BadSubscribe)
{
    const Env env;
    Inbox inbox;
    int id = 0;
    const int message = BIYOS_EVENT_MESSAGE;
    const int debug = BIYOS_LEVEL_DEBUG;
    EXPECT_EQ(biyos_subscribe(env.get(), nullptr, &inbox, message, debug, &id),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'subscriber': is null");
    EXPECT_EQ(biyos_subscribe(env.get(), receive, &inbox, 0, debug, &id), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: event kind 0: unknown");
    EXPECT_EQ(biyos_subscribe(env.get(), receive, &inbox, message, 9, &id), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: level 9: unknown");
    EXPECT_EQ(biyos_subscribe(env.get(), receive, &inbox, message, debug, nullptr),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'subscription': is null");
    EXPECT_EQ(inbox.calls, 0);
    EXPECT_EQ(id, 0);
}

TEST(CApiTest, NullSubscribe)
{
    int id = 0;
    EXPECT_EQ(listen(nullptr, receive, nullptr, BIYOS_LEVEL_DEBUG, &id), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(biyos_unsubscribe(nullptr, 1), BIYOS_INVALID_ARGUMENT);
}

TEST(CApiTest, Accessors)
{
    const biyos::Event event{
        .kind = biyos::EventKind::Message,
        .level = biyos::Level::Warning,
        .module = "presolve",
        .text = "removed rows",
        .fields = {{.name = "rows", .value = "3"}, {.name = "columns", .value = "1"}},
    };
    const biyos_event view{&event};
    EXPECT_EQ(biyos_event_kind(&view), BIYOS_EVENT_MESSAGE);
    EXPECT_EQ(biyos_event_level(&view), BIYOS_LEVEL_WARNING);
    EXPECT_STREQ(biyos_event_module(&view), "presolve");
    EXPECT_STREQ(biyos_event_text(&view), "removed rows");
    EXPECT_EQ(biyos_event_field_count(&view), 2);
    EXPECT_STREQ(biyos_event_field_name(&view, 0), "rows");
    EXPECT_STREQ(biyos_event_field_value(&view, 0), "3");
    EXPECT_STREQ(biyos_event_field_name(&view, 1), "columns");
    EXPECT_STREQ(biyos_event_field_value(&view, 1), "1");
    EXPECT_STREQ(biyos_event_field_name(&view, 2), "");
    EXPECT_STREQ(biyos_event_field_value(&view, -1), "");
}

TEST(CApiTest, NullEvent)
{
    EXPECT_EQ(biyos_event_kind(nullptr), 0);
    EXPECT_EQ(biyos_event_level(nullptr), 0);
    EXPECT_STREQ(biyos_event_module(nullptr), "");
    EXPECT_STREQ(biyos_event_text(nullptr), "");
    EXPECT_EQ(biyos_event_field_count(nullptr), 0);
    EXPECT_STREQ(biyos_event_field_name(nullptr, 0), "");
    EXPECT_STREQ(biyos_event_field_value(nullptr, 0), "");
}
