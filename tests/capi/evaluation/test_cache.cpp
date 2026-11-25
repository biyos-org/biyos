#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "handles.hpp"
#include "model/evaluation/evaluation.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace
{

using biyos::test::Env;
using biyos::test::Model;

void build(const Model &model)
{
    BIYOS_model &handle = *model.get();
    handle.evaluation =
        std::make_unique<biyos::Evaluation>(handle.handle.env(), handle.core, &handle);
}

}

TEST(CacheTest, Dropped)
{
    const Env env;
    const Model model(env);
    build(model);
    ASSERT_EQ(BIYOS_addvar(model.get(), 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(model.get()->evaluation, nullptr);
}

TEST(CacheTest, Kept)
{
    const Env env;
    const Model model(env);
    build(model);
    BIYOS_index n = -1;
    ASSERT_EQ(BIYOS_getnvar(model.get(), &n), BIYOS_OK);
    EXPECT_NE(model.get()->evaluation, nullptr);
}

TEST(CacheTest, Copied)
{
    const Env env;
    const Model model(env);
    build(model);
    const Model copy(env, model);
    EXPECT_EQ(copy.get()->evaluation, nullptr);
    EXPECT_NE(model.get()->evaluation, nullptr);
}
