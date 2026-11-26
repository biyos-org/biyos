#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "handles.hpp"

#include <gtest/gtest.h>

namespace
{

using biyos::test::Env;
using biyos::test::Model;

void build(const Model &model)
{
    model.get()->evaluation();
}

}

TEST(CacheTest, Dropped)
{
    const Env env;
    const Model model(env);
    build(model);
    ASSERT_EQ(BIYOS_addvar(model.get(), 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(model.get()->cached(), nullptr);
}

TEST(CacheTest, Kept)
{
    const Env env;
    const Model model(env);
    build(model);
    BIYOS_index n = -1;
    ASSERT_EQ(BIYOS_getnvar(model.get(), &n), BIYOS_OK);
    EXPECT_NE(model.get()->cached(), nullptr);
}

TEST(CacheTest, Copied)
{
    const Env env;
    const Model model(env);
    build(model);
    const Model copy(env, model);
    EXPECT_EQ(copy.get()->cached(), nullptr);
    EXPECT_NE(model.get()->cached(), nullptr);
}
