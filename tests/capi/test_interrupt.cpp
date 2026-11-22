#include "biyos/biyos.h"
#include "capi/entry.hpp"
#include "core/deadline.hpp"
#include "core/env.hpp"
#include "core/events.hpp"
#include "core/status.hpp"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <csignal>
#include <functional>
#include <future>
#include <limits>
#include <string>

namespace
{

using biyos::TerminationStatus;
using biyos::test::Env;
using biyos::test::Model;

constexpr long kPolls = 100;

void idle(biyos::Env &) {}

struct Loop
{
    std::function<void(biyos::Env &)> start = idle;
    long limit = (std::numeric_limits<long>::max)();
    long polls = 0;
    TerminationStatus outcome = TerminationStatus::NotSolved;
};

struct Stopper
{
    BIYOS_env *env;
    BIYOS_status interrupted = BIYOS_ERR_INTERNAL;
    BIYOS_status read = BIYOS_ERR_INTERNAL;
};

std::atomic<BIYOS_env *> signalled = nullptr;

void spin(biyos::Env &env, Loop *loop)
{
    const biyos::Deadline deadline(env.options(), env.clock(), env.interrupt());
    loop->start(env);
    while (loop->polls < loop->limit && !deadline.reached())
    {
        ++loop->polls;
    }
    loop->outcome = deadline.termination();
}

BIYOS_status run(const Env &env, Loop &loop)
{
    return biyos::capi::guarded<spin>(env.get(), &loop);
}

BIYOS_status interrupt_after(std::future<void> started, BIYOS_env *env)
{
    started.wait();
    return BIYOS_interrupt(env);
}

void on_signal(int)
{
    BIYOS_interrupt(signalled.load());
}

void raise_signal(biyos::Env &)
{
    std::raise(SIGINT);
}

void blank(biyos::Event &) {}

void announce(biyos::Env &env)
{
    env.events().emit(biyos::EventKind::Message, biyos::EventLevel::Info, blank);
}

void stop_call(const BIYOS_event *, void *data)
{
    auto *stopper = static_cast<Stopper *>(data);
    BIYOS_real value = 0.0;
    stopper->interrupted = BIYOS_interrupt(stopper->env);
    stopper->read = BIYOS_getdblopt(stopper->env, BIYOS_OPT_MAXTIME, &value);
}

void count(const BIYOS_event *, void *data)
{
    ++*static_cast<int *>(data);
}

BIYOS_status listen(const Env &env, BIYOS_hook hook, void *data)
{
    BIYOS_index id = 0;
    return BIYOS_addhook(env.get(), hook, data, BIYOS_EVT_MESSAGE, BIYOS_LVL_DBG, &id);
}

}

TEST(InterruptTest, OtherThread)
{
    const Env env;
    std::promise<void> started;
    auto other = std::async(std::launch::async, interrupt_after, started.get_future(), env.get());
    Loop loop{.start = [&started](biyos::Env &) { started.set_value(); }};
    EXPECT_EQ(run(env, loop), BIYOS_OK);
    EXPECT_EQ(other.get(), BIYOS_OK);
    EXPECT_EQ(loop.outcome, TerminationStatus::UserStop);
}

TEST(InterruptTest, SignalHandler)
{
    const Env env;
    signalled = env.get();
    const auto previous = std::signal(SIGINT, on_signal);
    ASSERT_NE(previous, SIG_ERR);
    Loop loop{.start = raise_signal, .limit = kPolls};
    const BIYOS_status status = run(env, loop);
    std::signal(SIGINT, previous);
    EXPECT_EQ(status, BIYOS_OK);
    EXPECT_EQ(loop.outcome, TerminationStatus::UserStop);
    EXPECT_EQ(loop.polls, 0);
}

TEST(InterruptTest, HookStop)
{
    const Env env;
    Stopper stopper{.env = env.get()};
    ASSERT_EQ(listen(env, stop_call, &stopper), BIYOS_OK);
    Loop loop{.start = announce, .limit = kPolls};
    EXPECT_EQ(run(env, loop), BIYOS_OK);
    EXPECT_EQ(stopper.interrupted, BIYOS_OK);
    EXPECT_EQ(stopper.read, BIYOS_OK);
    EXPECT_EQ(loop.outcome, TerminationStatus::UserStop);
    EXPECT_EQ(loop.polls, 0);
}

TEST(InterruptTest, NextCall)
{
    const Env env;
    const Model model(env);
    const biyos::Interrupt &flag = env.get()->core.interrupt();
    BIYOS_real value = 0.0;
    BIYOS_index n = 0;
    ASSERT_EQ(BIYOS_interrupt(env.get()), BIYOS_OK);
    EXPECT_TRUE(flag.is_set());
    EXPECT_EQ(BIYOS_getdblopt(env.get(), BIYOS_OPT_MAXTIME, &value), BIYOS_OK);
    EXPECT_FALSE(flag.is_set());
    ASSERT_EQ(BIYOS_interrupt(env.get()), BIYOS_OK);
    EXPECT_EQ(BIYOS_getnvar(model.get(), &n), BIYOS_OK);
    EXPECT_FALSE(flag.is_set());
}

TEST(InterruptTest, LateInterrupt)
{
    const Env env;
    Loop stopped{.start = [&env](biyos::Env &) { BIYOS_interrupt(env.get()); }, .limit = kPolls};
    ASSERT_EQ(run(env, stopped), BIYOS_OK);
    ASSERT_EQ(stopped.outcome, TerminationStatus::UserStop);
    ASSERT_EQ(BIYOS_interrupt(env.get()), BIYOS_OK);
    Loop next{.limit = kPolls};
    EXPECT_EQ(run(env, next), BIYOS_OK);
    EXPECT_EQ(next.polls, kPolls);
    EXPECT_EQ(next.outcome, TerminationStatus::NotSolved);
}

TEST(InterruptTest, Silent)
{
    const Env env;
    int events = 0;
    ASSERT_EQ(listen(env, count, &events), BIYOS_OK);
    ASSERT_EQ(BIYOS_setintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const std::string message = env.message();
    ASSERT_EQ(events, 1);
    EXPECT_EQ(BIYOS_interrupt(env.get()), BIYOS_OK);
    EXPECT_EQ(env.message(), message);
    EXPECT_EQ(events, 1);
}

TEST(InterruptTest, NullEnv)
{
    EXPECT_EQ(BIYOS_interrupt(nullptr), BIYOS_ERR_BADARG);
}
