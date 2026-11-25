#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>

namespace
{

using biyos::test::Env;
using biyos::test::last_message;
using biyos::test::Model;

struct Inside
{
    BIYOS_model *model;
    BIYOS_env *env;
    int result;
    BIYOS_status evaluated;
    BIYOS_status counted;
    BIYOS_status changed;
    BIYOS_status freed;
    BIYOS_status read;
    bool interrupted;
    std::string message;
};

struct Hooked
{
    BIYOS_model *model;
    bool done;
    BIYOS_status evaluated;
    BIYOS_status counted;
    BIYOS_status read;
};

int returning(const BIYOS_cbreq *, double *f, void *data)
{
    f[0] = 1.0;
    return static_cast<const Inside *>(data)->result;
}

int reentering(const BIYOS_cbreq *request, double *f, void *data)
{
    auto &inside = *static_cast<Inside *>(data);
    double nested = 0.0;
    BIYOS_offset nnz = -1;
    BIYOS_index n = -1;
    inside.evaluated = BIYOS_evalf(inside.model, request->x, &nested);
    inside.message = last_message(inside.env);
    inside.counted = BIYOS_getngpatrg(inside.model, 0, 1, &nnz);
    inside.changed = BIYOS_addvar(inside.model, 1, nullptr, nullptr, nullptr, nullptr);
    inside.freed = BIYOS_freemdl(inside.model);
    inside.read = BIYOS_getnvar(inside.model, &n);
    f[0] = 0.0;
    return 0;
}

int interrupting(const BIYOS_cbreq *, double *f, void *data)
{
    auto &inside = *static_cast<Inside *>(data);
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_interrupt(inside.env), BIYOS_OK);
    inside.read = BIYOS_getnvar(inside.model, &n);
    inside.interrupted = inside.model->env().interrupt().is_set();
    f[0] = 0.0;
    return 0;
}

void watch(const BIYOS_event *, void *data)
{
    auto &hooked = *static_cast<Hooked *>(data);
    if (hooked.done)
    {
        return;
    }
    hooked.done = true;
    const double x = 0.5;
    double f = 0.0;
    BIYOS_offset nnz = -1;
    BIYOS_index n = -1;
    hooked.evaluated = BIYOS_evalf(hooked.model, &x, &f);
    hooked.counted = BIYOS_getngpatrg(hooked.model, 0, 1, &nnz);
    hooked.read = BIYOS_getnvar(hooked.model, &n);
}

void cover(const Model &model, BIYOS_cbf f, Inside &inside)
{
    const BIYOS_index function = 0;
    BIYOS_index cb = -1;
    ASSERT_EQ(BIYOS_addvar(model.get(), 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model.get(), 1, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addcb(model.get(), 1, &function, &inside, &cb), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcbf(model.get(), cb, f), BIYOS_OK);
}

Inside inside_of(const Env &env, const Model &model, int result)
{
    return Inside{.model = model.get(),
                  .env = env.get(),
                  .result = result,
                  .evaluated = BIYOS_OK,
                  .counted = BIYOS_OK,
                  .changed = BIYOS_OK,
                  .freed = BIYOS_OK,
                  .read = BIYOS_ERR_INTERNAL,
                  .interrupted = false,
                  .message = {}};
}

}

TEST(EvaluationTest, Stopped)
{
    const Env env;
    const Model model(env);
    Inside inside = inside_of(env, model, -3);
    cover(model, returning, inside);
    const double x = 0.5;
    double f = -1.0;
    EXPECT_EQ(BIYOS_evalf(model.get(), &x, &f), BIYOS_ERR_CALLBACK);
    EXPECT_EQ(env.message(), "callback: callback 0: function returned -3");
}

TEST(EvaluationTest, Undefined)
{
    const Env env;
    const Model model(env);
    Inside inside = inside_of(env, model, 2);
    cover(model, returning, inside);
    const double x = 0.5;
    double f = -1.0;
    EXPECT_EQ(BIYOS_evalf(model.get(), &x, &f), BIYOS_OK);
    EXPECT_TRUE(std::isnan(f));
}

TEST(EvaluationTest, Absent)
{
    const Env env;
    const Model model(env);
    Inside inside = inside_of(env, model, 0);
    cover(model, returning, inside);
    const double x = 0.5;
    double g = -1.0;
    EXPECT_EQ(BIYOS_evalg(model.get(), &x, &g), BIYOS_ERR_UNSUPPORTED);
    EXPECT_EQ(env.message(), "unsupported: callback 0: has no Jacobian");
    const double w = 1.0;
    double hv = -1.0;
    EXPECT_EQ(BIYOS_evalhv(model.get(), &x, &w, &x, &hv), BIYOS_ERR_UNSUPPORTED);
    EXPECT_EQ(env.message(), "unsupported: callback 0: has no Hessian");
}

TEST(EvaluationTest, Inside)
{
    const Env env;
    const Model model(env);
    Inside inside = inside_of(env, model, 0);
    cover(model, reentering, inside);
    const double x = 0.5;
    double f = -1.0;
    ASSERT_EQ(BIYOS_evalf(model.get(), &x, &f), BIYOS_OK);
    EXPECT_EQ(inside.evaluated, BIYOS_ERR_BUSY);
    EXPECT_EQ(inside.message, "busy: model: is running a callback");
    EXPECT_EQ(inside.counted, BIYOS_ERR_BUSY);
    EXPECT_EQ(inside.changed, BIYOS_ERR_BUSY);
    EXPECT_EQ(inside.freed, BIYOS_ERR_BUSY);
    EXPECT_EQ(inside.read, BIYOS_OK);
    EXPECT_EQ(BIYOS_addvar(model.get(), 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
}

TEST(EvaluationTest, Interrupted)
{
    const Env env;
    const Model model(env);
    Inside inside = inside_of(env, model, 0);
    cover(model, interrupting, inside);
    const double x = 0.5;
    double f = -1.0;
    ASSERT_EQ(BIYOS_evalf(model.get(), &x, &f), BIYOS_OK);
    EXPECT_EQ(inside.read, BIYOS_OK);
    EXPECT_TRUE(inside.interrupted);
}

TEST(EvaluationTest, Hooked)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addvar(model.get(), 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model.get(), 1, nullptr), BIYOS_OK);
    Hooked hooked{.model = model.get(),
                  .done = false,
                  .evaluated = BIYOS_OK,
                  .counted = BIYOS_OK,
                  .read = BIYOS_ERR_INTERNAL};
    BIYOS_index hook = -1;
    ASSERT_EQ(
        BIYOS_addenvhook(env.get(), watch, &hooked, BIYOS_EVTKND_SPAN, BIYOS_EVTLVL_DBG, &hook),
        BIYOS_OK);
    const double x = 0.5;
    double f = -1.0;
    ASSERT_EQ(BIYOS_evalf(model.get(), &x, &f), BIYOS_OK);
    EXPECT_TRUE(hooked.done);
    EXPECT_EQ(hooked.evaluated, BIYOS_ERR_BADARG);
    EXPECT_EQ(hooked.counted, BIYOS_ERR_BADARG);
    EXPECT_EQ(hooked.read, BIYOS_OK);
}
