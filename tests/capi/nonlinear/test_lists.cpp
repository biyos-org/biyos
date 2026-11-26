#include "biyos/biyos.h"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::Model;

using Indices = std::vector<BIYOS_index>;

const std::string kBad = "invalid argument: ";

int succeed(const BIYOS_cbreq *, BIYOS_real *, void *)
{
    return 0;
}

void build(const Model &model, int variables, int functions)
{
    ASSERT_EQ(BIYOS_addvar(model.get(), variables, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model.get(), functions, nullptr), BIYOS_OK);
}

BIYOS_status add(const Model &model, BIYOS_index n, const BIYOS_index *var)
{
    const BIYOS_index function = 0;
    return BIYOS_addcb(model.get(), 1, &function, n, var, nullptr, nullptr);
}

BIYOS_index count_of(const Model &model, BIYOS_index cb)
{
    BIYOS_index n = -2;
    EXPECT_EQ(BIYOS_getncbvar(model.get(), cb, &n), BIYOS_OK);
    return n;
}

Indices read(const Model &model, BIYOS_index cb)
{
    Indices found(static_cast<std::size_t>(count_of(model, cb)), -1);
    EXPECT_EQ(BIYOS_getcbvar(model.get(), cb, count_of(model, cb), found.data()), BIYOS_OK);
    return found;
}

std::string failed(const Env &env, BIYOS_status status)
{
    EXPECT_EQ(status, BIYOS_ERR_BADARG);
    return env.message();
}

}

TEST(ListTest, Read)
{
    const Env env;
    const Model model(env);
    build(model, 4, 1);
    const Indices variables{3, 0, 2};
    ASSERT_EQ(add(model, 3, variables.data()), BIYOS_OK);
    ASSERT_EQ(add(model, 0, nullptr), BIYOS_OK);
    ASSERT_EQ(add(model, BIYOS_DENSE, nullptr), BIYOS_OK);
    EXPECT_EQ(count_of(model, 0), 3);
    EXPECT_EQ(read(model, 0), variables);
    EXPECT_EQ(count_of(model, 1), 0);
    EXPECT_EQ(BIYOS_getcbvar(model.get(), 1, 0, nullptr), BIYOS_OK);
    EXPECT_EQ(count_of(model, 2), BIYOS_DENSE);
    const Model copy(env, model);
    EXPECT_EQ(read(copy, 0), variables);
    EXPECT_EQ(count_of(copy, 2), BIYOS_DENSE);
}

TEST(ListTest, ReadRefused)
{
    const Env env;
    const Model model(env);
    build(model, 3, 1);
    const Indices variables{2, 0};
    ASSERT_EQ(add(model, 2, variables.data()), BIYOS_OK);
    ASSERT_EQ(add(model, BIYOS_DENSE, nullptr), BIYOS_OK);
    BIYOS_model *mdl = model.get();
    Indices found(2, -1);
    EXPECT_EQ(failed(env, BIYOS_getcbvar(mdl, 0, 1, found.data())),
              kBad + "argument 'cap': holds 1 of the 2 variables");
    EXPECT_EQ(failed(env, BIYOS_getcbvar(mdl, 0, -1, found.data())),
              kBad + "argument 'cap': -1 is negative");
    EXPECT_EQ(failed(env, BIYOS_getcbvar(mdl, 1, 2, found.data())),
              kBad + "callback 1: reads every variable");
    EXPECT_EQ(failed(env, BIYOS_getcbvar(mdl, 2, 2, found.data())), kBad + "callback 2: unknown");
    EXPECT_EQ(found, Indices(2, -1));
    BIYOS_index n = -2;
    EXPECT_EQ(failed(env, BIYOS_getncbvar(mdl, 2, &n)), kBad + "callback 2: unknown");
    EXPECT_EQ(failed(env, BIYOS_getncbvar(mdl, 0, nullptr)), kBad + "argument 'n': is null");
    EXPECT_EQ(BIYOS_getncbvar(nullptr, 0, &n), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getcbvar(nullptr, 0, 2, found.data()), BIYOS_ERR_BADARG);
    EXPECT_EQ(n, -2);
}

TEST(ListTest, AddRefused)
{
    const Env env;
    const Model model(env);
    build(model, 3, 1);
    const Indices repeated{1, 2, 1};
    const Indices unknown{0, 3};
    const BIYOS_index variable = 0;
    const std::string argument = kBad + "argument '";
    EXPECT_EQ(failed(env, add(model, -2, nullptr)), argument + "n': -2 is negative");
    EXPECT_EQ(failed(env, add(model, 1, nullptr)), argument + "var': is null");
    EXPECT_EQ(failed(env, add(model, BIYOS_DENSE, &variable)),
              argument + "var': is not null for BIYOS_DENSE");
    EXPECT_EQ(failed(env, add(model, 3, repeated.data())),
              argument + "var': variable 1 is repeated");
    EXPECT_EQ(failed(env, add(model, 2, unknown.data())), kBad + "variable 3: unknown");
    BIYOS_index n = -2;
    EXPECT_EQ(failed(env, BIYOS_getncbvar(model.get(), 0, &n)), kBad + "callback 0: unknown");
}

TEST(ListTest, Unlisted)
{
    const Env env;
    const Model model(env);
    build(model, 3, 1);
    const Indices variables{2, 0};
    ASSERT_EQ(add(model, 2, variables.data()), BIYOS_OK);
    BIYOS_model *mdl = model.get();
    const Indices functions{0, 0};
    const Indices columns{0, 1};
    EXPECT_EQ(failed(env, BIYOS_setcbg(mdl, 0, 2, functions.data(), columns.data(), succeed)),
              kBad + "callback 0: Jacobian of function 0: variable 1 is not listed");
    EXPECT_EQ(failed(env, BIYOS_setcbh(mdl, 0, 2, columns.data(), columns.data(), succeed)),
              kBad + "callback 0: Hessian variable 1 is not listed");
    EXPECT_EQ(BIYOS_setcbg(mdl, 0, 2, functions.data(), variables.data(), succeed), BIYOS_OK);
    EXPECT_EQ(BIYOS_setcbh(mdl, 0, 1, &variables[1], &variables[0], succeed), BIYOS_OK);
}
