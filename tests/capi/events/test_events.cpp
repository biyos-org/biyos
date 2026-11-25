#include "biyos/biyos.h"
#include "capi/env/event.hpp"
#include "core/events/events.hpp"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

class Env
{
  public:
    Env() { EXPECT_EQ(BIYOS_newenv(&pointer_), BIYOS_OK); }
    ~Env() { BIYOS_freeenv(pointer_); }
    Env(const Env &) = delete;
    Env &operator=(const Env &) = delete;

    BIYOS_env *get() const { return pointer_; }
    std::string message() const { return biyos::test::last_message(pointer_); }

  private:
    BIYOS_env *pointer_ = nullptr;
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
    BIYOS_model *model = nullptr;
    int calls = 0;
    BIYOS_status status = BIYOS_OK;
    std::string message;
};

void receive(const BIYOS_event *event, void *data)
{
    Inbox *inbox = static_cast<Inbox *>(data);
    ++inbox->calls;
    inbox->kind = BIYOS_getevtknd(event);
    inbox->level = BIYOS_getevtlvl(event);
    inbox->module = BIYOS_getevtmod(event);
    inbox->text = BIYOS_getevttext(event);
    inbox->fields = BIYOS_getnevtfld(event);
    inbox->name = BIYOS_getevtfldname(event, 0);
    inbox->value = BIYOS_getevtfldval(event, 0);
}

void source(const BIYOS_event *event, void *data)
{
    static_cast<std::vector<const BIYOS_model *> *>(data)->push_back(BIYOS_getevtmdl(event));
}

void delete_unknown(const BIYOS_event *, void *data)
{
    Probe *probe = static_cast<Probe *>(data);
    ++probe->calls;
    probe->status = BIYOS_delenvhook(probe->env, 999);
    probe->message = biyos::test::last_message(probe->env);
}

void refuse(const BIYOS_event *, void *data)
{
    Probe *probe = static_cast<Probe *>(data);
    ++probe->calls;
    probe->status = BIYOS_setenvintopt(probe->env, BIYOS_OPT_SEED, 42);
    probe->message = biyos::test::last_message(probe->env);
}

void delete_own(const BIYOS_event *, void *data)
{
    Probe *probe = static_cast<Probe *>(data);
    ++probe->calls;
    probe->status = BIYOS_delmdlhook(probe->model, 999);
    probe->message = biyos::test::last_message(probe->env);
}

void read_error(const BIYOS_event *, void *data)
{
    Probe *probe = static_cast<Probe *>(data);
    ++probe->calls;
    probe->message = biyos::test::last_message(probe->env);
}

void free_inside(const BIYOS_event *, void *data)
{
    Probe *probe = static_cast<Probe *>(data);
    ++probe->calls;
    probe->status = BIYOS_freeenv(probe->env);
    probe->message = biyos::test::last_message(probe->env);
}

BIYOS_status listen(BIYOS_env *env, BIYOS_hook hook, void *data, int level, BIYOS_index *id)
{
    return BIYOS_addenvhook(env, hook, data, BIYOS_EVTKND_MSG, level, id);
}

BIYOS_status listen_to(BIYOS_model *model, BIYOS_hook hook, void *data, int level, BIYOS_index *id)
{
    return BIYOS_addmdlhook(model, hook, data, BIYOS_EVTKND_MSG, level, id);
}

void fail(const Env &env)
{
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
}

void fail(const biyos::test::Model &model)
{
    EXPECT_EQ(BIYOS_getnvar(model.get(), nullptr), BIYOS_ERR_BADARG);
}

void count(const BIYOS_event *, void *data)
{
    ++*static_cast<int *>(data);
}

struct Mark
{
    std::vector<std::string> *heard;
    std::string name;
};

void stamp(const BIYOS_event *, void *data)
{
    const Mark &seen = *static_cast<const Mark *>(data);
    seen.heard->push_back(seen.name);
}

struct Attempt
{
    BIYOS_env *env = nullptr;
    BIYOS_model *model = nullptr;
    BIYOS_model *other = nullptr;
    std::vector<BIYOS_status> statuses;
    std::vector<std::string> messages;
    int seed = -1;
};

void note(Attempt &attempt, BIYOS_status status)
{
    attempt.statuses.push_back(status);
    attempt.messages.emplace_back(biyos::test::last_message(attempt.env));
}

void change(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_env *env = attempt.env;
    BIYOS_index id = 0;
    note(attempt, BIYOS_setenvboolopt(env, BIYOS_OPT_PRESOLVE, 0));
    note(attempt, BIYOS_setenvintopt(env, BIYOS_OPT_SEED, 42));
    note(attempt, BIYOS_setenvdblopt(env, BIYOS_OPT_MAXTIME, 1.0));
    note(attempt, BIYOS_setenvstropt(env, BIYOS_OPT_REPRO, "replay.txt"));
    note(attempt, listen(env, receive, nullptr, BIYOS_EVTLVL_DBG, &id));
    note(attempt, BIYOS_delenvhook(env, 1));
    note(attempt, BIYOS_delenvopt(env, BIYOS_OPT_SEED));
    note(attempt, BIYOS_getenvintopt(env, BIYOS_OPT_SEED, &attempt.seed));
}

void change_model(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_model *model = attempt.model;
    BIYOS_model *made = nullptr;
    BIYOS_index id = 0;
    note(attempt, BIYOS_setmdlintopt(model, BIYOS_OPT_SEED, 42));
    note(attempt, listen_to(model, receive, nullptr, BIYOS_EVTLVL_DBG, &id));
    note(attempt, BIYOS_delmdlhook(model, 1));
    note(attempt, BIYOS_addvar(model, 1, nullptr, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_freemdl(model));
    note(attempt, BIYOS_setmdlintopt(attempt.other, BIYOS_OPT_SEED, 42));
    note(attempt, BIYOS_setenvintopt(attempt.env, BIYOS_OPT_SEED, 42));
    note(attempt, BIYOS_newmdl(attempt.env, &made));
    note(attempt, BIYOS_getmdlintopt(model, BIYOS_OPT_SEED, &attempt.seed));
}

}

TEST(CApiTest, AddHook)
{
    const Env env;
    Inbox inbox;
    BIYOS_index id = 0;
    ASSERT_EQ(listen(env.get(), receive, &inbox, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(id, 1);
    fail(env);
    EXPECT_EQ(inbox.calls, 1);
    EXPECT_EQ(inbox.kind, BIYOS_EVTKND_MSG);
    EXPECT_EQ(inbox.level, BIYOS_EVTLVL_ERR);
    EXPECT_EQ(inbox.module, "capi");
    EXPECT_EQ(inbox.text, "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(inbox.text, env.message());
    EXPECT_EQ(inbox.fields, 1);
    EXPECT_EQ(inbox.name, "code");
    EXPECT_EQ(inbox.value, "-100");
}

TEST(CApiTest, DeleteHook)
{
    const Env env;
    Inbox inbox;
    BIYOS_index id = 0;
    ASSERT_EQ(listen(env.get(), receive, &inbox, BIYOS_EVTLVL_DBG, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    fail(env);
    EXPECT_EQ(inbox.calls, 0);
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: hook 1: unknown");
}

TEST(CApiTest, ReadOnly)
{
    const Env env;
    Attempt attempt;
    attempt.env = env.get();
    BIYOS_index id = 0;
    ASSERT_EQ(listen(env.get(), change, &attempt, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    fail(env);
    const BIYOS_status refused = BIYOS_ERR_BADARG;
    const std::vector<BIYOS_status>
        statuses{refused, refused, refused, refused, refused, refused, refused, BIYOS_OK};
    const std::string reason =
        "invalid argument: environment: is read-only while delivering an event";
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages, std::vector<std::string>(8, reason));
    EXPECT_EQ(attempt.seed, 0);
    BIYOS_int seed = -1;
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_SEED, &seed), BIYOS_OK);
    EXPECT_EQ(seed, 0);
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
}

TEST(CApiTest, NestedFailure)
{
    const Env env;
    Probe probe;
    probe.env = env.get();
    BIYOS_index id = 0;
    ASSERT_EQ(listen(env.get(), delete_unknown, &probe, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    fail(env);
    EXPECT_EQ(probe.calls, 1);
    EXPECT_EQ(probe.status, BIYOS_ERR_BADARG);
    EXPECT_EQ(probe.message,
              "invalid argument: environment: is read-only while delivering an event");
    EXPECT_EQ(env.message(), "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
}

TEST(CApiTest, TwoHooks)
{
    const Env env;
    Probe first;
    Probe second;
    first.env = env.get();
    second.env = env.get();
    BIYOS_index id = 0;
    ASSERT_EQ(listen(env.get(), refuse, &first, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    ASSERT_EQ(listen(env.get(), read_error, &second, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    fail(env);
    const std::string failure = "invalid argument: option 'seed': -1 is outside [0, 2147483647]";
    EXPECT_EQ(first.status, BIYOS_ERR_BADARG);
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
    ASSERT_EQ(listen(env.get(), free_inside, &probe, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    fail(env);
    EXPECT_EQ(probe.calls, 1);
    EXPECT_EQ(probe.status, BIYOS_ERR_BADARG);
    EXPECT_EQ(probe.message,
              "invalid argument: environment: is read-only while delivering an event");
    BIYOS_int seed = -1;
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, 5), BIYOS_OK);
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_SEED, &seed), BIYOS_OK);
    EXPECT_EQ(seed, 5);
}

TEST(CApiTest, BadHook)
{
    const Env env;
    Inbox inbox;
    BIYOS_index id = 0;
    const int message = BIYOS_EVTKND_MSG;
    const int debug = BIYOS_EVTLVL_DBG;
    EXPECT_EQ(BIYOS_addenvhook(env.get(), nullptr, &inbox, message, debug, &id), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'hk': is null");
    EXPECT_EQ(BIYOS_addenvhook(env.get(), receive, &inbox, 0, debug, &id), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: event kind 0: unknown");
    EXPECT_EQ(BIYOS_addenvhook(env.get(), receive, &inbox, message, 9, &id), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: level 9: unknown");
    EXPECT_EQ(BIYOS_addenvhook(env.get(), receive, &inbox, message, debug, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(inbox.calls, 0);
    EXPECT_EQ(id, 0);
}

TEST(CApiTest, NullHook)
{
    BIYOS_index id = 0;
    EXPECT_EQ(listen(nullptr, receive, nullptr, BIYOS_EVTLVL_DBG, &id), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delenvhook(nullptr, 1), BIYOS_ERR_BADARG);
    EXPECT_EQ(listen_to(nullptr, receive, nullptr, BIYOS_EVTLVL_DBG, &id), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delmdlhook(nullptr, 1), BIYOS_ERR_BADARG);
}

TEST(CApiTest, Accessors)
{
    const biyos::Event event{
        .kind = biyos::EventKind::Message,
        .level = biyos::EventLevel::Warning,
        .time = {},
        .module = "presolve",
        .text = "removed rows",
        .fields = {{.name = "rows", .value = "3"}, {.name = "columns", .value = "1"}},
    };
    const BIYOS_event view{&event};
    EXPECT_EQ(BIYOS_getevtknd(&view), BIYOS_EVTKND_MSG);
    EXPECT_EQ(BIYOS_getevtlvl(&view), BIYOS_EVTLVL_WARN);
    EXPECT_STREQ(BIYOS_getevtmod(&view), "presolve");
    EXPECT_STREQ(BIYOS_getevttext(&view), "removed rows");
    EXPECT_EQ(BIYOS_getnevtfld(&view), 2);
    EXPECT_STREQ(BIYOS_getevtfldname(&view, 0), "rows");
    EXPECT_STREQ(BIYOS_getevtfldval(&view, 0), "3");
    EXPECT_STREQ(BIYOS_getevtfldname(&view, 1), "columns");
    EXPECT_STREQ(BIYOS_getevtfldval(&view, 1), "1");
    EXPECT_STREQ(BIYOS_getevtfldname(&view, 2), "");
    EXPECT_STREQ(BIYOS_getevtfldval(&view, -1), "");
    EXPECT_EQ(BIYOS_getevtmdl(&view), nullptr);
}

TEST(CApiTest, NullEvent)
{
    EXPECT_EQ(BIYOS_getevtknd(nullptr), 0);
    EXPECT_EQ(BIYOS_getevtlvl(nullptr), 0);
    EXPECT_STREQ(BIYOS_getevtmod(nullptr), "");
    EXPECT_STREQ(BIYOS_getevttext(nullptr), "");
    EXPECT_EQ(BIYOS_getnevtfld(nullptr), 0);
    EXPECT_STREQ(BIYOS_getevtfldname(nullptr, 0), "");
    EXPECT_STREQ(BIYOS_getevtfldval(nullptr, 0), "");
    EXPECT_EQ(BIYOS_getevtmdl(nullptr), nullptr);
}

TEST(CApiTest, EventModel)
{
    const biyos::test::Env env;
    const biyos::test::Model model(env);
    std::vector<const BIYOS_model *> seen;
    BIYOS_index id = 0;
    ASSERT_EQ(listen(env.get(), source, &seen, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_getnvar(model.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const std::vector<const BIYOS_model *> expected{model.get(), nullptr};
    EXPECT_EQ(seen, expected);
}

TEST(CApiTest, ModelHook)
{
    const biyos::test::Env env;
    const biyos::test::Model first(env);
    const biyos::test::Model second(env);
    std::vector<const BIYOS_model *> local;
    std::vector<const BIYOS_model *> shared;
    BIYOS_index id = 0;
    ASSERT_EQ(listen_to(first.get(), source, &local, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    ASSERT_EQ(listen(env.get(), source, &shared, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    fail(first);
    fail(second);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(local, std::vector<const BIYOS_model *>{first.get()});
    const std::vector<const BIYOS_model *> all{first.get(), second.get(), nullptr};
    EXPECT_EQ(shared, all);
}

TEST(CApiTest, ModelMessage)
{
    const biyos::test::Env env;
    const biyos::test::Model model(env);
    Inbox inbox;
    BIYOS_index id = 0;
    ASSERT_EQ(listen_to(model.get(), receive, &inbox, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    fail(model);
    EXPECT_EQ(inbox.calls, 1);
    EXPECT_EQ(inbox.kind, BIYOS_EVTKND_MSG);
    EXPECT_EQ(inbox.level, BIYOS_EVTLVL_ERR);
    EXPECT_EQ(inbox.module, "capi");
    EXPECT_EQ(inbox.text, env.message());
    EXPECT_EQ(inbox.name, "code");
    EXPECT_EQ(inbox.value, "-100");
}

TEST(CApiTest, ModelOrder)
{
    const biyos::test::Env env;
    const biyos::test::Model model(env);
    std::vector<std::string> heard;
    Mark shared{&heard, "environment"};
    Mark first{&heard, "first"};
    Mark second{&heard, "second"};
    BIYOS_index id = 0;
    ASSERT_EQ(listen(env.get(), stamp, &shared, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    ASSERT_EQ(listen_to(model.get(), stamp, &first, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    ASSERT_EQ(listen_to(model.get(), stamp, &second, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    fail(model);
    EXPECT_EQ(heard, (std::vector<std::string>{"first", "second", "environment"}));
}

TEST(CApiTest, ModelIds)
{
    const biyos::test::Env env;
    const biyos::test::Model first(env);
    const biyos::test::Model second(env);
    int heard = 0;
    std::vector<BIYOS_index> ids(5, 0);
    ASSERT_EQ(listen_to(first.get(), count, &heard, BIYOS_EVTLVL_DBG, &ids[0]), BIYOS_OK);
    ASSERT_EQ(listen_to(first.get(), count, &heard, BIYOS_EVTLVL_DBG, &ids[1]), BIYOS_OK);
    ASSERT_EQ(listen_to(second.get(), count, &heard, BIYOS_EVTLVL_DBG, &ids[2]), BIYOS_OK);
    ASSERT_EQ(listen(env.get(), count, &heard, BIYOS_EVTLVL_DBG, &ids[3]), BIYOS_OK);
    EXPECT_EQ(BIYOS_delmdlhook(first.get(), 1), BIYOS_OK);
    ASSERT_EQ(listen_to(first.get(), count, &heard, BIYOS_EVTLVL_DBG, &ids[4]), BIYOS_OK);
    EXPECT_EQ(ids, (std::vector<BIYOS_index>{1, 2, 1, 1, 3}));
    EXPECT_EQ(BIYOS_delmdlhook(first.get(), 1), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: hook 1: unknown");
    EXPECT_EQ(BIYOS_delmdlhook(second.get(), 2), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delmdlhook(second.get(), 1), BIYOS_OK);
}

TEST(CApiTest, ModelArguments)
{
    const biyos::test::Env env;
    const biyos::test::Model model(env);
    Inbox inbox;
    BIYOS_index id = 0;
    const int message = BIYOS_EVTKND_MSG;
    const int debug = BIYOS_EVTLVL_DBG;
    EXPECT_EQ(BIYOS_addmdlhook(model.get(), nullptr, &inbox, message, debug, &id),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'hk': is null");
    EXPECT_EQ(BIYOS_addmdlhook(model.get(), receive, &inbox, 0, debug, &id), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: event kind 0: unknown");
    EXPECT_EQ(BIYOS_addmdlhook(model.get(), receive, &inbox, message, 9, &id), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: level 9: unknown");
    EXPECT_EQ(BIYOS_addmdlhook(model.get(), receive, &inbox, message, debug, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    fail(model);
    EXPECT_EQ(inbox.calls, 0);
    EXPECT_EQ(id, 0);
}

TEST(CApiTest, ModelFree)
{
    const biyos::test::Env env;
    int heard = 0;
    BIYOS_model *model = nullptr;
    BIYOS_index id = 0;
    ASSERT_EQ(BIYOS_newmdl(env.get(), &model), BIYOS_OK);
    ASSERT_EQ(listen_to(model, count, &heard, BIYOS_EVTLVL_DBG, &id), BIYOS_OK);
    ASSERT_EQ(BIYOS_freemdl(model), BIYOS_OK);
    const biyos::test::Model next(env);
    fail(next);
    EXPECT_EQ(heard, 0);
    ASSERT_EQ(listen_to(next.get(), count, &heard, BIYOS_EVTLVL_DBG, &id), BIYOS_OK);
    EXPECT_EQ(id, 1);
}

TEST(CApiTest, ModelCopy)
{
    const biyos::test::Env env;
    const biyos::test::Env other;
    const biyos::test::Model model(env);
    int heard = 0;
    int copied = 0;
    BIYOS_index id = 0;
    ASSERT_EQ(listen_to(model.get(), count, &heard, BIYOS_EVTLVL_DBG, &id), BIYOS_OK);
    const biyos::test::Model near(env, model);
    const biyos::test::Model far(other, model);
    fail(near);
    fail(far);
    EXPECT_EQ(heard, 0);
    ASSERT_EQ(listen_to(near.get(), count, &copied, BIYOS_EVTLVL_DBG, &id), BIYOS_OK);
    EXPECT_EQ(id, 1);
    fail(model);
    EXPECT_EQ(heard, 1);
    EXPECT_EQ(copied, 0);
}

TEST(CApiTest, ModelReadOnly)
{
    const biyos::test::Env env;
    const biyos::test::Model model(env);
    const biyos::test::Model other(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    attempt.other = other.get();
    BIYOS_index id = 0;
    ASSERT_EQ(listen_to(model.get(), change_model, &attempt, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    fail(model);
    const BIYOS_status refused = BIYOS_ERR_BADARG;
    const std::vector<BIYOS_status>
        statuses{refused, refused, refused, refused, refused, refused, refused, refused, BIYOS_OK};
    const std::string own = "invalid argument: model: is read-only while delivering an event";
    const std::string shared =
        "invalid argument: environment: is read-only while delivering an event";
    const std::vector<std::string> messages{own, own, own, own, own, own, shared, shared, shared};
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages, messages);
    EXPECT_EQ(attempt.seed, 0);
    BIYOS_index variables = -1;
    EXPECT_EQ(BIYOS_getnvar(model.get(), &variables), BIYOS_OK);
    EXPECT_EQ(variables, 0);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': is null");
    EXPECT_EQ(BIYOS_delmdlhook(model.get(), id), BIYOS_OK);
}

TEST(CApiTest, ModelError)
{
    const biyos::test::Env env;
    const biyos::test::Model model(env);
    Probe probe;
    probe.env = env.get();
    probe.model = model.get();
    BIYOS_index id = 0;
    ASSERT_EQ(listen_to(model.get(), delete_own, &probe, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    fail(model);
    const BIYOS_error error = biyos::test::last_error(env.get());
    EXPECT_EQ(probe.calls, 1);
    EXPECT_EQ(probe.status, BIYOS_ERR_BADARG);
    EXPECT_EQ(probe.message, "invalid argument: model: is read-only while delivering an event");
    EXPECT_EQ(error.status, BIYOS_ERR_BADARG);
    EXPECT_EQ(error.mdl, model.get());
    EXPECT_STREQ(error.msg, "invalid argument: argument 'n': is null");
}

TEST(CApiTest, ModelSilent)
{
    const biyos::test::Env env;
    const biyos::test::Model model(env);
    Probe probe;
    probe.env = env.get();
    probe.model = model.get();
    int heard = 0;
    int shared = 0;
    BIYOS_index id = 0;
    ASSERT_EQ(listen_to(model.get(), delete_own, &probe, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    ASSERT_EQ(listen_to(model.get(), count, &heard, BIYOS_EVTLVL_DBG, &id), BIYOS_OK);
    ASSERT_EQ(listen(env.get(), count, &shared, BIYOS_EVTLVL_DBG, &id), BIYOS_OK);
    fail(model);
    EXPECT_EQ(probe.calls, 1);
    EXPECT_EQ(heard, 1);
    EXPECT_EQ(shared, 1);
}
