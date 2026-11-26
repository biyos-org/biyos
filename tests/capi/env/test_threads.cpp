#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "core/env.hpp"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <future>
#include <latch>
#include <map>
#include <memory>
#include <thread>
#include <vector>

using biyos::capi::guarded;
using biyos::capi::writable;
using biyos::test::last_error;
using biyos::test::Model;

namespace
{

constexpr std::size_t kThreads = 4;
constexpr BIYOS_index kRounds = 100;

struct Built
{
    BIYOS_index variables = 0;
    std::ptrdiff_t binaries = 0;
    BIYOS_status freed = BIYOS_ERR_INTERNAL;
};

class Tally
{
  public:
    void count(const BIYOS_model *model)
    {
        if (inside_.exchange(true))
        {
            ++overlaps_;
        }
        std::this_thread::yield();
        ++counts_[model];
        inside_.store(false);
    }

    int overlaps() const { return overlaps_.load(); }
    int of(const BIYOS_model *model) const
    {
        const auto found = counts_.find(model);
        return found == counts_.end() ? 0 : found->second;
    }

  private:
    std::atomic<bool> inside_{false};
    std::atomic<int> overlaps_{0};
    std::map<const BIYOS_model *, int> counts_;
};

void tally(const BIYOS_event *event, void *data)
{
    static_cast<Tally *>(data)->count(BIYOS_getevtmdl(event));
}

struct Gate
{
    std::latch inside{1};
    std::latch done{1};
};

void park(Gate *gate)
{
    gate->inside.count_down();
    gate->done.wait();
}

void hold_env(biyos::Env &, Gate *gate)
{
    park(gate);
}

void change_env(biyos::Env &env, Gate *gate)
{
    writable(env);
    park(gate);
}

void hold_model(const BIYOS_model &, Gate *gate)
{
    park(gate);
}

void change_model(BIYOS_model &model, Gate *gate)
{
    writable(model);
    park(gate);
}

template <auto Body, typename Target> BIYOS_status run(Target *target, Gate *gate)
{
    return guarded<Body>(target, gate);
}

class Held
{
  public:
    template <typename Target>
    Held(BIYOS_status (*call)(Target *, Gate *), Target *target)
        : status_(std::async(std::launch::async, call, target, &gate_))
    {
        gate_.inside.wait();
    }

    ~Held() { release(); }

    Held(const Held &) = delete;
    Held &operator=(const Held &) = delete;

    BIYOS_status release()
    {
        if (status_.valid())
        {
            gate_.done.count_down();
            returned_ = status_.get();
        }
        return returned_;
    }

  private:
    Gate gate_;
    std::future<BIYOS_status> status_;
    BIYOS_status returned_ = BIYOS_ERR_INTERNAL;
};

BIYOS_status add_one(const Model &model)
{
    return BIYOS_addvar(model.get(), 1, nullptr, nullptr, nullptr, nullptr);
}

void build(BIYOS_env *env, std::latch &start, Built &built)
{
    BIYOS_model *model = nullptr;
    EXPECT_EQ(BIYOS_newmdl(env, &model), BIYOS_OK);
    start.arrive_and_wait();
    const BIYOS_int binary = BIYOS_VARKND_BIN;
    for (BIYOS_index round = 0; round < kRounds; ++round)
    {
        EXPECT_EQ(BIYOS_addvar(model, 1, &binary, nullptr, nullptr, nullptr), BIYOS_OK);
    }
    EXPECT_EQ(BIYOS_getnvar(model, &built.variables), BIYOS_OK);
    std::vector<BIYOS_int> kinds(static_cast<std::size_t>(built.variables));
    EXPECT_EQ(BIYOS_getvarkndrg(model, 0, built.variables, kinds.data()), BIYOS_OK);
    built.binaries = std::ranges::count(kinds, BIYOS_VARKND_BIN);
    built.freed = BIYOS_freemdl(model);
}

void fail_often(BIYOS_model *model, std::latch &start, const BIYOS_env *env)
{
    start.arrive_and_wait();
    for (BIYOS_index round = 0; round < kRounds; ++round)
    {
        EXPECT_EQ(BIYOS_getnvar(model, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(last_error(env).mdl, model);
    }
}

}

TEST(ThreadTest, Models)
{
    const biyos::test::Env env;
    std::latch start(kThreads);
    std::vector<Built> built(kThreads);
    std::vector<std::thread> threads;
    for (Built &each : built)
    {
        threads.emplace_back(build, env.get(), std::ref(start), std::ref(each));
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    for (const Built &each : built)
    {
        EXPECT_EQ(each.variables, kRounds);
        EXPECT_EQ(each.binaries, kRounds);
        EXPECT_EQ(each.freed, BIYOS_OK);
    }
}

TEST(ThreadTest, Hooks)
{
    Tally seen;
    const biyos::test::Env env;
    BIYOS_index hook = 0;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), tally, &seen, BIYOS_EVTKND_MSG, BIYOS_EVTLVL_ERR, &hook),
              BIYOS_OK);
    std::vector<std::unique_ptr<Model>> models;
    std::latch start(kThreads);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < kThreads; ++i)
    {
        models.push_back(std::make_unique<Model>(env));
        threads.emplace_back(fail_often, models.back()->get(), std::ref(start), env.get());
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(seen.overlaps(), 0);
    for (const std::unique_ptr<Model> &model : models)
    {
        EXPECT_EQ(seen.of(model->get()), kRounds);
    }
}

TEST(ThreadTest, ModelHooks)
{
    Tally shared;
    std::array<Tally, 2> local;
    const biyos::test::Env env;
    const Model first(env);
    const Model second(env);
    const std::array<BIYOS_model *, 2> models{first.get(), second.get()};
    BIYOS_index id = 0;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), tally, &shared, BIYOS_EVTKND_MSG, BIYOS_EVTLVL_ERR, &id),
              BIYOS_OK);
    for (std::size_t m = 0; m < models.size(); ++m)
    {
        ASSERT_EQ(
            BIYOS_addmdlhook(models[m], tally, &local[m], BIYOS_EVTKND_MSG, BIYOS_EVTLVL_ERR, &id),
            BIYOS_OK);
    }
    std::latch start(kThreads);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < kThreads; ++i)
    {
        threads.emplace_back(fail_often, models[i % 2], std::ref(start), env.get());
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(shared.overlaps(), 0);
    for (std::size_t m = 0; m < models.size(); ++m)
    {
        EXPECT_EQ(local[m].overlaps(), 0);
        EXPECT_EQ(local[m].of(models[m]), 2 * kRounds);
        EXPECT_EQ(local[m].of(models[1 - m]), 0);
        EXPECT_EQ(shared.of(models[m]), 2 * kRounds);
    }
}

TEST(ThreadTest, EnvBusy)
{
    const biyos::test::Env env;
    const Model model(env);
    Held held(run<hold_env, BIYOS_env>, env.get());
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, 1), BIYOS_ERR_BUSY);
    EXPECT_EQ(env.message(), "busy: environment: is in a call on another thread");
    EXPECT_EQ(last_error(env.get()).mdl, nullptr);
    BIYOS_index hook = 0;
    EXPECT_EQ(
        BIYOS_addenvhook(env.get(), tally, nullptr, BIYOS_EVTKND_MSG, BIYOS_EVTLVL_ERR, &hook),
        BIYOS_ERR_BUSY);
    EXPECT_EQ(BIYOS_delenvhook(env.get(), 1), BIYOS_ERR_BUSY);
    EXPECT_EQ(BIYOS_delenvopt(env.get(), BIYOS_OPT_SEED), BIYOS_ERR_BUSY);
    EXPECT_EQ(BIYOS_freeenv(env.get()), BIYOS_ERR_BUSY);
    BIYOS_int seed = -1;
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_SEED, &seed), BIYOS_OK);
    EXPECT_EQ(seed, 0);
    EXPECT_EQ(held.release(), BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, 1), BIYOS_OK);
}

TEST(ThreadTest, EnvChanging)
{
    const biyos::test::Env env;
    const Model model(env);
    Held held(run<change_env, BIYOS_env>, env.get());
    BIYOS_int seed = -1;
    EXPECT_EQ(BIYOS_getenvintopt(env.get(), BIYOS_OPT_SEED, &seed), BIYOS_ERR_BUSY);
    EXPECT_EQ(seed, -1);
    EXPECT_EQ(env.message(), "busy: environment: is being changed on another thread");
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnvar(model.get(), &n), BIYOS_ERR_BUSY);
    EXPECT_EQ(last_error(env.get()).mdl, model.get());
    BIYOS_model *made = nullptr;
    EXPECT_EQ(BIYOS_newmdl(env.get(), &made), BIYOS_ERR_BUSY);
    EXPECT_EQ(made, nullptr);
    EXPECT_EQ(held.release(), BIYOS_OK);
    EXPECT_EQ(BIYOS_getnvar(model.get(), &n), BIYOS_OK);
    EXPECT_EQ(n, 0);
}

TEST(ThreadTest, ModelBusy)
{
    const biyos::test::Env env;
    const Model model(env);
    const Model other(env);
    Held held(run<hold_model, BIYOS_model>, model.get());
    EXPECT_EQ(add_one(model), BIYOS_ERR_BUSY);
    EXPECT_EQ(env.message(), "busy: model: is in a call on another thread");
    EXPECT_EQ(last_error(env.get()).mdl, model.get());
    EXPECT_EQ(BIYOS_freemdl(model.get()), BIYOS_ERR_BUSY);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, 1), BIYOS_ERR_BUSY);
    EXPECT_EQ(BIYOS_delcbrg(model.get(), 0, 0, nullptr), BIYOS_ERR_BUSY);
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnvar(model.get(), &n), BIYOS_OK);
    EXPECT_EQ(n, 0);
    EXPECT_EQ(add_one(other), BIYOS_OK);
    BIYOS_model *made = nullptr;
    EXPECT_EQ(BIYOS_newmdl(env.get(), &made), BIYOS_OK);
    EXPECT_EQ(BIYOS_freemdl(made), BIYOS_OK);
    EXPECT_EQ(held.release(), BIYOS_OK);
    EXPECT_EQ(add_one(model), BIYOS_OK);
}

TEST(ThreadTest, OptionBusy)
{
    const biyos::test::Env env;
    const Model model(env);
    BIYOS_model *busy = model.get();
    Held held(run<hold_model, BIYOS_model>, busy);
    EXPECT_EQ(BIYOS_setmdlboolopt(busy, BIYOS_OPT_PRESOLVE, 0), BIYOS_ERR_BUSY);
    EXPECT_EQ(env.message(), "busy: model: is in a call on another thread");
    EXPECT_EQ(last_error(env.get()).mdl, busy);
    EXPECT_EQ(BIYOS_setmdlintopt(busy, BIYOS_OPT_SEED, 1), BIYOS_ERR_BUSY);
    EXPECT_EQ(BIYOS_setmdldblopt(busy, BIYOS_OPT_MAXTIME, 1.0), BIYOS_ERR_BUSY);
    EXPECT_EQ(BIYOS_setmdlstropt(busy, BIYOS_OPT_REPRO, "busy.txt"), BIYOS_ERR_BUSY);
    EXPECT_EQ(BIYOS_setmdlflgopt(busy, BIYOS_OPT_SEED, 0, 0), BIYOS_ERR_BUSY);
    EXPECT_EQ(BIYOS_delmdlopt(busy, BIYOS_OPT_SEED), BIYOS_ERR_BUSY);
    BIYOS_int seed = -1;
    EXPECT_EQ(BIYOS_getmdlintopt(busy, BIYOS_OPT_SEED, &seed), BIYOS_OK);
    EXPECT_EQ(held.release(), BIYOS_OK);
    EXPECT_EQ(BIYOS_setmdlintopt(busy, BIYOS_OPT_SEED, 1), BIYOS_OK);
}

TEST(ThreadTest, OtherHeld)
{
    const biyos::test::Env env;
    const Model model(env);
    const Model other(env);
    BIYOS_model *idle = model.get();
    Held held(run<change_model, BIYOS_model>, other.get());
    EXPECT_EQ(BIYOS_setmdlboolopt(idle, BIYOS_OPT_PRESOLVE, 0), BIYOS_OK);
    EXPECT_EQ(BIYOS_setmdlintopt(idle, BIYOS_OPT_SEED, 1), BIYOS_OK);
    EXPECT_EQ(BIYOS_setmdldblopt(idle, BIYOS_OPT_MAXTIME, 1.0), BIYOS_OK);
    EXPECT_EQ(BIYOS_setmdlstropt(idle, BIYOS_OPT_REPRO, "idle.txt"), BIYOS_OK);
    BIYOS_int seed = -1;
    EXPECT_EQ(BIYOS_getmdlintopt(idle, BIYOS_OPT_SEED, &seed), BIYOS_OK);
    EXPECT_EQ(seed, 1);
    EXPECT_EQ(BIYOS_delmdlopt(idle, BIYOS_OPT_SEED), BIYOS_OK);
    EXPECT_EQ(held.release(), BIYOS_OK);
}

TEST(ThreadTest, ModelChanging)
{
    const biyos::test::Env env;
    const Model model(env);
    Held held(run<change_model, BIYOS_model>, model.get());
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnvar(model.get(), &n), BIYOS_ERR_BUSY);
    EXPECT_EQ(n, -1);
    EXPECT_EQ(env.message(), "busy: model: is being changed on another thread");
    EXPECT_EQ(add_one(model), BIYOS_ERR_BUSY);
    BIYOS_model *copy = nullptr;
    EXPECT_EQ(BIYOS_copymdl(model.get(), env.get(), &copy), BIYOS_ERR_BUSY);
    EXPECT_EQ(copy, nullptr);
    EXPECT_EQ(held.release(), BIYOS_OK);
    EXPECT_EQ(BIYOS_getnvar(model.get(), &n), BIYOS_OK);
    EXPECT_EQ(n, 0);
}

TEST(ThreadTest, CopyTarget)
{
    const biyos::test::Env source;
    const biyos::test::Env target;
    const Model model(source);
    Held held(run<change_env, BIYOS_env>, target.get());
    BIYOS_model *copy = nullptr;
    EXPECT_EQ(BIYOS_copymdl(model.get(), target.get(), &copy), BIYOS_ERR_BUSY);
    EXPECT_EQ(copy, nullptr);
    EXPECT_EQ(source.message(), "busy: environment: is being changed on another thread");
    EXPECT_EQ(last_error(source.get()).mdl, model.get());
    EXPECT_EQ(held.release(), BIYOS_OK);
    EXPECT_EQ(BIYOS_copymdl(model.get(), target.get(), &copy), BIYOS_OK);
    EXPECT_EQ(BIYOS_freemdl(copy), BIYOS_OK);
}
