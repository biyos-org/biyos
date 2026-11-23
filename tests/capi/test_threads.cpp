#include "biyos/biyos.h"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <latch>
#include <map>
#include <memory>
#include <thread>
#include <vector>

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

void build(BIYOS_env *env, std::latch &start, Built &built)
{
    BIYOS_model *model = nullptr;
    EXPECT_EQ(BIYOS_newmdl(env, &model), BIYOS_OK);
    start.arrive_and_wait();
    const BIYOS_int binary = BIYOS_VAR_BIN;
    for (BIYOS_index round = 0; round < kRounds; ++round)
    {
        EXPECT_EQ(BIYOS_addvar(model, 1, &binary, nullptr, nullptr, nullptr), BIYOS_OK);
    }
    EXPECT_EQ(BIYOS_getnvar(model, &built.variables), BIYOS_OK);
    std::vector<BIYOS_int> kinds(static_cast<std::size_t>(built.variables));
    EXPECT_EQ(BIYOS_getvarkndrg(model, 0, built.variables, kinds.data()), BIYOS_OK);
    built.binaries = std::ranges::count(kinds, BIYOS_VAR_BIN);
    built.freed = BIYOS_freemdl(model);
}

void fail_often(BIYOS_model *model, std::latch &start, const BIYOS_env *env)
{
    start.arrive_and_wait();
    for (BIYOS_index round = 0; round < kRounds; ++round)
    {
        EXPECT_EQ(BIYOS_getnvar(model, nullptr), BIYOS_ERR_BADARG);
        EXPECT_EQ(biyos::test::last_error(env).mdl, model);
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
    ASSERT_EQ(BIYOS_addenvhook(env.get(), tally, &seen, BIYOS_EVT_MESSAGE, BIYOS_LVL_ERR, &hook),
              BIYOS_OK);
    std::vector<std::unique_ptr<biyos::test::Model>> models;
    std::latch start(kThreads);
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < kThreads; ++i)
    {
        models.push_back(std::make_unique<biyos::test::Model>(env));
        threads.emplace_back(fail_often, models.back()->get(), std::ref(start), env.get());
    }
    for (std::thread &thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(seen.overlaps(), 0);
    for (const std::unique_ptr<biyos::test::Model> &model : models)
    {
        EXPECT_EQ(seen.of(model->get()), kRounds);
    }
}
