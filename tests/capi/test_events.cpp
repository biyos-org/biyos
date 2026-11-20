#include "biyos/biyos.h"
#include "capi/event.hpp"
#include "core/events.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

class Env
{
  public:
    Env() { EXPECT_EQ(BIYOS_newenv(&ptr_), BIYOS_OK); }
    ~Env() { BIYOS_freeenv(ptr_); }
    Env(const Env &) = delete;
    Env &operator=(const Env &) = delete;

    BIYOS_env *get() const { return ptr_; }
    std::string message() const { return BIYOS_lasterror(ptr_); }

  private:
    BIYOS_env *ptr_ = nullptr;
};

struct Inbox
{
    int calls = 0;
    int kind = 0;
    int level = 0;
    std::string module;
    std::string text;
    int fields = 0;
    std::string name;
    std::string value;
};

struct Probe
{
    BIYOS_env *env = nullptr;
    int calls = 0;
    BIYOS_status status = BIYOS_OK;
    std::string message;
};

void receive(const BIYOS_evt *event, void *data)
{
    Inbox *inbox = static_cast<Inbox *>(data);
    ++inbox->calls;
    inbox->kind = BIYOS_getevtkind(event);
    inbox->level = BIYOS_getevtlvl(event);
    inbox->module = BIYOS_getevtmodule(event);
    inbox->text = BIYOS_getevttext(event);
    inbox->fields = BIYOS_getnevtfields(event);
    inbox->name = BIYOS_getevtfieldname(event, 0);
    inbox->value = BIYOS_getevtfieldvalue(event, 0);
}

void unsubscribe_unknown(const BIYOS_evt *, void *data)
{
    Probe *probe = static_cast<Probe *>(data);
    ++probe->calls;
    probe->status = BIYOS_delhook(probe->env, 999);
    probe->message = BIYOS_lasterror(probe->env);
}

void refuse(const BIYOS_evt *, void *data)
{
    Probe *probe = static_cast<Probe *>(data);
    ++probe->calls;
    probe->status = BIYOS_setintopt(probe->env, BIYOS_OPT_SEED, 42);
    probe->message = BIYOS_lasterror(probe->env);
}

void read_error(const BIYOS_evt *, void *data)
{
    Probe *probe = static_cast<Probe *>(data);
    ++probe->calls;
    probe->message = BIYOS_lasterror(probe->env);
}

void free_inside(const BIYOS_evt *, void *data)
{
    Probe *probe = static_cast<Probe *>(data);
    ++probe->calls;
    probe->status = BIYOS_freeenv(probe->env);
    probe->message = BIYOS_lasterror(probe->env);
}

BIYOS_status listen(BIYOS_env *env, BIYOS_hook subscriber, void *data, int lowest, BIYOS_index *id)
{
    return BIYOS_addhook(env, subscriber, data, BIYOS_EVT_MESSAGE, lowest, id);
}

void fail(const Env &env)
{
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_INVALIDARG);
}

struct Attempt
{
    BIYOS_env *env = nullptr;
    std::vector<BIYOS_status> statuses;
    std::vector<std::string> messages;
    int seed = -1;
};

void note(Attempt &attempt, BIYOS_status status)
{
    attempt.statuses.push_back(status);
    attempt.messages.emplace_back(BIYOS_lasterror(attempt.env));
}

void change(const BIYOS_evt *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_env *env = attempt.env;
    BIYOS_index id = 0;
    note(attempt, BIYOS_setboolopt(env, BIYOS_OPT_PRESOLVE, 0));
    note(attempt, BIYOS_setintopt(env, BIYOS_OPT_SEED, 42));
    note(attempt, BIYOS_setdblopt(env, BIYOS_OPT_MAXTIME, 1.0));
    note(attempt, BIYOS_setstropt(env, BIYOS_OPT_REPRO, "replay.txt"));
    note(attempt, listen(env, receive, nullptr, BIYOS_LVL_DBG, &id));
    note(attempt, BIYOS_delhook(env, 1));
    note(attempt, BIYOS_getintopt(env, BIYOS_OPT_SEED, &attempt.seed));
}

}

TEST(CApiTest, Subscribe)
{
    const Env env;
    Inbox inbox;
    BIYOS_index id = 0;
    ASSERT_EQ(listen(env.get(), receive, &inbox, BIYOS_LVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(id, 1);
    fail(env);
    EXPECT_EQ(inbox.calls, 1);
    EXPECT_EQ(inbox.kind, BIYOS_EVT_MESSAGE);
    EXPECT_EQ(inbox.level, BIYOS_LVL_ERR);
    EXPECT_EQ(inbox.module, "capi");
    EXPECT_EQ(inbox.text, "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(inbox.text, env.message());
    EXPECT_EQ(inbox.fields, 1);
    EXPECT_EQ(inbox.name, "code");
    EXPECT_EQ(inbox.value, "-100");
}

TEST(CApiTest, Unsubscribe)
{
    const Env env;
    Inbox inbox;
    BIYOS_index id = 0;
    ASSERT_EQ(listen(env.get(), receive, &inbox, BIYOS_LVL_DBG, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_delhook(env.get(), id), BIYOS_OK);
    fail(env);
    EXPECT_EQ(inbox.calls, 0);
    EXPECT_EQ(BIYOS_delhook(env.get(), id), BIYOS_ERR_INVALIDARG);
    EXPECT_EQ(env.message(), "invalid argument: subscription 1: unknown");
}

TEST(CApiTest, ReadOnly)
{
    const Env env;
    Attempt attempt;
    attempt.env = env.get();
    BIYOS_index id = 0;
    ASSERT_EQ(listen(env.get(), change, &attempt, BIYOS_LVL_ERR, &id), BIYOS_OK);
    fail(env);
    const BIYOS_status refused = BIYOS_ERR_INVALIDARG;
    const std::vector<BIYOS_status>
        statuses{refused, refused, refused, refused, refused, refused, BIYOS_OK};
    const std::string reason =
        "invalid argument: environment: is read-only while delivering an event";
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages, std::vector<std::string>(7, reason));
    EXPECT_EQ(attempt.seed, 0);
    BIYOS_int seed = -1;
    EXPECT_EQ(BIYOS_getintopt(env.get(), BIYOS_OPT_SEED, &seed), BIYOS_OK);
    EXPECT_EQ(seed, 0);
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(BIYOS_delhook(env.get(), id), BIYOS_OK);
}

TEST(CApiTest, NestedFailure)
{
    const Env env;
    Probe probe;
    probe.env = env.get();
    BIYOS_index id = 0;
    ASSERT_EQ(listen(env.get(), unsubscribe_unknown, &probe, BIYOS_LVL_ERR, &id), BIYOS_OK);
    fail(env);
    EXPECT_EQ(probe.calls, 1);
    EXPECT_EQ(probe.status, BIYOS_ERR_INVALIDARG);
    EXPECT_EQ(probe.message,
              "invalid argument: environment: is read-only while delivering an event");
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
}

TEST(CApiTest, TwoSubscribers)
{
    const Env env;
    Probe first;
    Probe second;
    first.env = env.get();
    second.env = env.get();
    BIYOS_index id = 0;
    ASSERT_EQ(listen(env.get(), refuse, &first, BIYOS_LVL_ERR, &id), BIYOS_OK);
    ASSERT_EQ(listen(env.get(), read_error, &second, BIYOS_LVL_ERR, &id), BIYOS_OK);
    fail(env);
    const std::string failure = "invalid argument: option 'seed': -1 is outside [0, 2147483647]";
    EXPECT_EQ(first.status, BIYOS_ERR_INVALIDARG);
    EXPECT_EQ(first.message,
              "invalid argument: environment: is read-only while delivering an event");
    EXPECT_EQ(second.calls, 1);
    EXPECT_EQ(second.message, failure);
    EXPECT_EQ(env.message(), failure);
}

TEST(CApiTest, BusyFree)
{
    const Env env;
    Probe probe;
    probe.env = env.get();
    BIYOS_index id = 0;
    ASSERT_EQ(listen(env.get(), free_inside, &probe, BIYOS_LVL_ERR, &id), BIYOS_OK);
    fail(env);
    EXPECT_EQ(probe.calls, 1);
    EXPECT_EQ(probe.status, BIYOS_ERR_INVALIDARG);
    EXPECT_EQ(probe.message,
              "invalid argument: environment: is read-only while delivering an event");
    BIYOS_int seed = -1;
    EXPECT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, 5), BIYOS_OK);
    EXPECT_EQ(BIYOS_getintopt(env.get(), BIYOS_OPT_SEED, &seed), BIYOS_OK);
    EXPECT_EQ(seed, 5);
}

TEST(CApiTest, BadSubscribe)
{
    const Env env;
    Inbox inbox;
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    const int debug = BIYOS_LVL_DBG;
    EXPECT_EQ(BIYOS_addhook(env.get(), nullptr, &inbox, message, debug, &id), BIYOS_ERR_INVALIDARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'hook': is null");
    EXPECT_EQ(BIYOS_addhook(env.get(), receive, &inbox, 0, debug, &id), BIYOS_ERR_INVALIDARG);
    EXPECT_EQ(env.message(), "invalid argument: event kind 0: unknown");
    EXPECT_EQ(BIYOS_addhook(env.get(), receive, &inbox, message, 9, &id), BIYOS_ERR_INVALIDARG);
    EXPECT_EQ(env.message(), "invalid argument: level 9: unknown");
    EXPECT_EQ(BIYOS_addhook(env.get(), receive, &inbox, message, debug, nullptr),
              BIYOS_ERR_INVALIDARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'id': is null");
    EXPECT_EQ(inbox.calls, 0);
    EXPECT_EQ(id, 0);
}

TEST(CApiTest, NullSubscribe)
{
    BIYOS_index id = 0;
    EXPECT_EQ(listen(nullptr, receive, nullptr, BIYOS_LVL_DBG, &id), BIYOS_ERR_INVALIDARG);
    EXPECT_EQ(BIYOS_delhook(nullptr, 1), BIYOS_ERR_INVALIDARG);
}

TEST(CApiTest, Accessors)
{
    const biyos::Event event{
        .kind = biyos::EventKind::Message,
        .level = biyos::Level::Warning,
        .time = {},
        .module = "presolve",
        .text = "removed rows",
        .fields = {{.name = "rows", .value = "3"}, {.name = "columns", .value = "1"}},
    };
    const BIYOS_evt view{&event};
    EXPECT_EQ(BIYOS_getevtkind(&view), BIYOS_EVT_MESSAGE);
    EXPECT_EQ(BIYOS_getevtlvl(&view), BIYOS_LVL_WARN);
    EXPECT_STREQ(BIYOS_getevtmodule(&view), "presolve");
    EXPECT_STREQ(BIYOS_getevttext(&view), "removed rows");
    EXPECT_EQ(BIYOS_getnevtfields(&view), 2);
    EXPECT_STREQ(BIYOS_getevtfieldname(&view, 0), "rows");
    EXPECT_STREQ(BIYOS_getevtfieldvalue(&view, 0), "3");
    EXPECT_STREQ(BIYOS_getevtfieldname(&view, 1), "columns");
    EXPECT_STREQ(BIYOS_getevtfieldvalue(&view, 1), "1");
    EXPECT_STREQ(BIYOS_getevtfieldname(&view, 2), "");
    EXPECT_STREQ(BIYOS_getevtfieldvalue(&view, -1), "");
}

TEST(CApiTest, NullEvent)
{
    EXPECT_EQ(BIYOS_getevtkind(nullptr), 0);
    EXPECT_EQ(BIYOS_getevtlvl(nullptr), 0);
    EXPECT_STREQ(BIYOS_getevtmodule(nullptr), "");
    EXPECT_STREQ(BIYOS_getevttext(nullptr), "");
    EXPECT_EQ(BIYOS_getnevtfields(nullptr), 0);
    EXPECT_STREQ(BIYOS_getevtfieldname(nullptr, 0), "");
    EXPECT_STREQ(BIYOS_getevtfieldvalue(nullptr, 0), "");
}
