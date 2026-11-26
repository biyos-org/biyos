#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "core/memory/usage.hpp"
#include "core/numbers.hpp"
#include "handles.hpp"
#include "model/compare/compare.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace
{

using biyos::Callback;
using biyos::difference;
using biyos::Index;
using biyos::IndexPair;
using biyos::Request;
using biyos::Usage;
using biyos::test::Env;
using biyos::test::last_message;
using biyos::test::Model;

const std::string kBad = "invalid argument: ";

struct Seen
{
    std::size_t size = 0;
    const BIYOS_model *model = nullptr;
    BIYOS_index callback = -1;
    BIYOS_index thread = -1;
    const BIYOS_real *point = nullptr;
    const BIYOS_real *weights = nullptr;
    const BIYOS_real *direction = nullptr;
    int calls = 0;
};

int record(const BIYOS_cbreq *req, BIYOS_real *out, void *data)
{
    Seen &seen = *static_cast<Seen *>(data);
    seen.size = req->size;
    seen.model = req->mdl;
    seen.callback = req->cb;
    seen.thread = req->thread;
    seen.point = req->x;
    seen.weights = req->w;
    seen.direction = req->v;
    ++seen.calls;
    out[0] = 0.5;
    return 0;
}

int undefined(const BIYOS_cbreq *, BIYOS_real *, void *)
{
    return 1;
}

int stopped(const BIYOS_cbreq *, BIYOS_real *, void *)
{
    return -1;
}

struct Attempt
{
    BIYOS_env *env = nullptr;
    BIYOS_model *model = nullptr;
    std::vector<BIYOS_status> statuses;
    std::vector<std::string> messages;
    BIYOS_status read = BIYOS_ERR_INTERNAL;
    void *data = nullptr;
};

void build(const Model &model, int variables, int functions)
{
    ASSERT_EQ(BIYOS_addvar(model.get(), variables, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model.get(), functions, nullptr), BIYOS_OK);
}

BIYOS_index add(const Model &model, const std::vector<BIYOS_index> &functions, void *data)
{
    BIYOS_index cb = -1;
    const auto n = static_cast<BIYOS_index>(functions.size());
    EXPECT_EQ(BIYOS_addcb(model.get(), n, functions.data(), BIYOS_DENSE, nullptr, data, &cb),
              BIYOS_OK);
    return cb;
}

std::string failed(const Env &env, BIYOS_status status)
{
    EXPECT_EQ(status, BIYOS_ERR_BADARG);
    return env.message();
}

std::string in_use(const Env &env, BIYOS_status status)
{
    EXPECT_EQ(status, BIYOS_ERR_INUSE);
    return env.message();
}

const Callback &held(const Model &model, BIYOS_index cb)
{
    return model.get()->model().callback(cb);
}

void *data_of(const Model &model, BIYOS_index cb)
{
    void *data = nullptr;
    EXPECT_EQ(BIYOS_getcbdata(model.get(), cb, &data), BIYOS_OK);
    return data;
}

BIYOS_index count_of(const Model &model, BIYOS_index cb)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getncbfun(model.get(), cb, &n), BIYOS_OK);
    return n;
}

void attach_exp(const Model &model, BIYOS_index function)
{
    BIYOS_model *mdl = model.get();
    BIYOS_offset x = -1;
    BIYOS_offset node = -1;
    ASSERT_EQ(BIYOS_addvarnode(mdl, 0, &x), BIYOS_OK);
    ASSERT_EQ(BIYOS_addnode(mdl, BIYOS_OP_EXP, 1, &x, nullptr, &node), BIYOS_OK);
    ASSERT_EQ(BIYOS_attachexprsp(mdl, 1, &function, &node), BIYOS_OK);
}

void cover(const Model &model)
{
    build(model, 1, 4);
    add(model, {2}, nullptr);
    add(model, {1, 2}, nullptr);
    add(model, {3, 1}, nullptr);
}

std::string deleted(const Env &env, const Model &model, BIYOS_index function)
{
    std::vector<BIYOS_index> map(4, 9);
    const BIYOS_status status = BIYOS_delfunsp(model.get(), 1, &function, map.data());
    EXPECT_EQ(map, std::vector<BIYOS_index>(4, 9));
    return in_use(env, status);
}

std::vector<BIYOS_index> covered_by(const Model &model)
{
    std::vector<BIYOS_index> found;
    const biyos::Model &stored = model.get()->model();
    for (Index cb = 0; cb < stored.n_callbacks(); ++cb)
    {
        found.push_back(stored.callback(cb).covered.front());
    }
    return found;
}

std::string drop_refusal(const Env &env, const Model &model, const std::vector<BIYOS_index> &idx)
{
    std::vector<BIYOS_index> map(3, 9);
    const auto n = static_cast<BIYOS_index>(idx.size());
    const BIYOS_status status = BIYOS_delcbsp(model.get(), n, idx.data(), map.data());
    EXPECT_EQ(map, std::vector<BIYOS_index>(3, 9));
    return failed(env, status);
}

std::string range_refusal(const Env &env, const Model &model, BIYOS_index beg, BIYOS_index end)
{
    return failed(env, BIYOS_delcbrg(model.get(), beg, end, nullptr));
}

std::uint64_t bits(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

void note(Attempt &attempt, BIYOS_status status)
{
    attempt.statuses.push_back(status);
    attempt.messages.emplace_back(last_message(attempt.env));
}

void change_all(Attempt &attempt)
{
    BIYOS_model *model = attempt.model;
    const BIYOS_index function = 1;
    note(attempt, BIYOS_addcb(model, 1, &function, BIYOS_DENSE, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_setcbf(model, 0, record));
    note(attempt, BIYOS_setcbg(model, 0, 0, nullptr, nullptr, record));
    note(attempt, BIYOS_setcbh(model, 0, 0, nullptr, nullptr, record));
    note(attempt, BIYOS_setcbhv(model, 0, record));
    note(attempt, BIYOS_setcbdata(model, 0, nullptr));
    attempt.read = BIYOS_getcbdata(model, 0, &attempt.data);
}

void change(const BIYOS_event *, void *data)
{
    change_all(*static_cast<Attempt *>(data));
}

}

TEST(CallbackTest, Registered)
{
    const Env env;
    const Model model(env);
    build(model, 3, 3);
    int tag = 0;
    EXPECT_EQ(add(model, {2, 0}, &tag), 0);
    const BIYOS_index function = 1;
    EXPECT_EQ(BIYOS_addcb(model.get(), 1, &function, BIYOS_DENSE, nullptr, nullptr, nullptr),
              BIYOS_OK);
    const std::vector<BIYOS_index> fun{0, 2, 0};
    const std::vector<BIYOS_index> var{1, 0, 1};
    const std::vector<BIYOS_index> var1{1, 0, 1};
    const std::vector<BIYOS_index> var2{2, 0, 2};
    BIYOS_model *mdl = model.get();
    EXPECT_EQ(BIYOS_setcbf(mdl, 0, record), BIYOS_OK);
    EXPECT_EQ(BIYOS_setcbg(mdl, 0, 3, fun.data(), var.data(), record), BIYOS_OK);
    EXPECT_EQ(BIYOS_setcbh(mdl, 0, 3, var1.data(), var2.data(), record), BIYOS_OK);
    EXPECT_EQ(BIYOS_setcbhv(mdl, 0, record), BIYOS_OK);
    EXPECT_EQ(env.message(), "");
    EXPECT_EQ(mdl->model().n_callbacks(), 2);
    EXPECT_EQ(held(model, 1).covered, (std::vector<Index>{1}));
    const Callback &first = held(model, 0);
    EXPECT_EQ(first.covered, (std::vector<Index>{2, 0}));
    EXPECT_EQ(first.jacobian_pairs, (std::vector<IndexPair>{{1, 1}, {0, 0}, {1, 1}}));
    EXPECT_EQ(first.hessian_pairs, (std::vector<IndexPair>{{1, 2}, {0, 0}, {1, 2}}));
    EXPECT_TRUE(first.function && first.jacobian && first.hessian && first.hessian_product);
    EXPECT_EQ(data_of(model, 0), &tag);
    EXPECT_EQ(data_of(model, 1), nullptr);
    EXPECT_FALSE(held(model, 1).function);
}

TEST(CallbackTest, Nulls)
{
    const Env env;
    const Model model(env);
    build(model, 3, 2);
    add(model, {0, 1}, nullptr);
    BIYOS_model *mdl = model.get();
    const std::string null = kBad + "argument '";
    EXPECT_EQ(failed(env, BIYOS_setcbf(mdl, 0, nullptr)), null + "f': is null");
    EXPECT_EQ(failed(env, BIYOS_setcbg(mdl, 0, 0, nullptr, nullptr, nullptr)),
              null + "g': is null");
    EXPECT_EQ(failed(env, BIYOS_setcbh(mdl, 0, 0, nullptr, nullptr, nullptr)),
              null + "h': is null");
    EXPECT_EQ(failed(env, BIYOS_setcbhv(mdl, 0, nullptr)), null + "hv': is null");
    EXPECT_EQ(failed(env, BIYOS_getcbdata(mdl, 0, nullptr)), null + "data': is null");
    EXPECT_EQ(failed(env, BIYOS_getncbfun(mdl, 0, nullptr)), null + "n': is null");
    EXPECT_EQ(BIYOS_getcbfun(mdl, 0, 2, nullptr), BIYOS_OK);
    EXPECT_EQ(failed(env, BIYOS_addcb(mdl, 1, nullptr, BIYOS_DENSE, nullptr, nullptr, nullptr)),
              null + "fun': is null");
    EXPECT_EQ(failed(env, BIYOS_addcb(mdl, -1, nullptr, BIYOS_DENSE, nullptr, nullptr, nullptr)),
              null + "m': -1 is negative");
    EXPECT_EQ(failed(env, BIYOS_addcb(mdl, 0, nullptr, BIYOS_DENSE, nullptr, nullptr, nullptr)),
              null + "m': 0 is not positive");
    const BIYOS_index variable = 0;
    EXPECT_EQ(failed(env, BIYOS_setcbg(mdl, 0, 1, nullptr, &variable, record)),
              null + "fun': is null");
    EXPECT_EQ(failed(env, BIYOS_setcbg(mdl, 0, 1, &variable, nullptr, record)),
              null + "var': is null");
    EXPECT_EQ(failed(env, BIYOS_setcbg(mdl, 0, -1, nullptr, nullptr, record)),
              null + "nnz': -1 is negative");
    EXPECT_EQ(failed(env, BIYOS_setcbh(mdl, 0, 1, nullptr, &variable, record)),
              null + "var1': is null");
    EXPECT_EQ(failed(env, BIYOS_setcbh(mdl, 0, 1, &variable, nullptr, record)),
              null + "var2': is null");
    EXPECT_EQ(failed(env, BIYOS_setcbh(mdl, 0, -1, nullptr, nullptr, record)),
              null + "nnz': -1 is negative");
    EXPECT_EQ(BIYOS_addcb(nullptr, 1, &variable, BIYOS_DENSE, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getcbdata(nullptr, 0, nullptr), BIYOS_ERR_BADARG);
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getncbfun(nullptr, 0, &n), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getcbfun(nullptr, 0, 1, &n), BIYOS_ERR_BADARG);
    EXPECT_EQ(n, -1);
    EXPECT_EQ(BIYOS_setcbg(mdl, 0, 0, nullptr, nullptr, record), BIYOS_OK);
    EXPECT_EQ(BIYOS_setcbh(mdl, 0, 0, nullptr, nullptr, record), BIYOS_OK);
    EXPECT_EQ(mdl->model().n_callbacks(), 1);
    const Callback &first = held(model, 0);
    EXPECT_TRUE(first.jacobian_pairs.empty());
    EXPECT_TRUE(first.hessian_pairs.empty());
    EXPECT_FALSE(first.function);
}

TEST(CallbackTest, Unknown)
{
    const Env env;
    const Model model(env);
    build(model, 1, 1);
    add(model, {0}, nullptr);
    BIYOS_model *mdl = model.get();
    const std::string unknown = kBad + "callback 1: unknown";
    void *data = nullptr;
    EXPECT_EQ(failed(env, BIYOS_setcbf(mdl, 1, record)), unknown);
    EXPECT_EQ(failed(env, BIYOS_setcbg(mdl, 1, 0, nullptr, nullptr, record)), unknown);
    EXPECT_EQ(failed(env, BIYOS_setcbh(mdl, 1, 0, nullptr, nullptr, record)), unknown);
    EXPECT_EQ(failed(env, BIYOS_setcbhv(mdl, 1, record)), unknown);
    EXPECT_EQ(failed(env, BIYOS_setcbdata(mdl, 1, nullptr)), unknown);
    EXPECT_EQ(failed(env, BIYOS_getcbdata(mdl, 1, &data)), unknown);
    BIYOS_index n = -1;
    EXPECT_EQ(failed(env, BIYOS_getncbfun(mdl, 1, &n)), unknown);
    EXPECT_EQ(failed(env, BIYOS_getcbfun(mdl, 1, 1, &n)), unknown);
    EXPECT_EQ(failed(env, BIYOS_getcbfun(mdl, -1, 1, &n)), kBad + "callback -1: unknown");
    EXPECT_EQ(n, -1);
    EXPECT_EQ(failed(env, BIYOS_setcbf(mdl, -1, record)), kBad + "callback -1: unknown");
    const BIYOS_index function = 3;
    EXPECT_EQ(failed(env, BIYOS_addcb(mdl, 1, &function, BIYOS_DENSE, nullptr, nullptr, nullptr)),
              kBad + "function 3: unknown");
}

TEST(CallbackTest, Covered)
{
    const Env env;
    const Model model(env);
    build(model, 1, 4);
    add(model, {2, 0, 3}, nullptr);
    add(model, {1}, nullptr);
    EXPECT_EQ(count_of(model, 0), 3);
    EXPECT_EQ(count_of(model, 1), 1);
    std::vector<BIYOS_index> fun(4, -1);
    EXPECT_EQ(BIYOS_getcbfun(model.get(), 0, 4, fun.data()), BIYOS_OK);
    EXPECT_EQ(fun, (std::vector<BIYOS_index>{2, 0, 3, -1}));
    EXPECT_EQ(BIYOS_getcbfun(model.get(), 1, 1, fun.data()), BIYOS_OK);
    EXPECT_EQ(fun, (std::vector<BIYOS_index>{1, 0, 3, -1}));
}

TEST(CallbackTest, Capacity)
{
    const Env env;
    const Model model(env);
    build(model, 1, 3);
    add(model, {2, 0, 1}, nullptr);
    BIYOS_model *mdl = model.get();
    std::vector<BIYOS_index> fun(3, -1);
    EXPECT_EQ(failed(env, BIYOS_getcbfun(mdl, 0, 2, fun.data())),
              kBad + "argument 'cap': holds 2 of the 3 functions");
    EXPECT_EQ(failed(env, BIYOS_getcbfun(mdl, 0, 0, nullptr)),
              kBad + "argument 'cap': holds 0 of the 3 functions");
    EXPECT_EQ(failed(env, BIYOS_getcbfun(mdl, 0, -1, fun.data())),
              kBad + "argument 'cap': -1 is negative");
    EXPECT_EQ(fun, std::vector<BIYOS_index>(3, -1));
}

TEST(CallbackTest, Empty)
{
    const Env env;
    const Model model(env);
    build(model, 1, 1);
    BIYOS_index n = -1;
    EXPECT_EQ(failed(env, BIYOS_getncbfun(model.get(), 0, &n)), kBad + "callback 0: unknown");
    EXPECT_EQ(failed(env, BIYOS_getcbfun(model.get(), 0, 0, nullptr)),
              kBad + "callback 0: unknown");
    EXPECT_EQ(n, -1);
}

TEST(CallbackTest, Patterns)
{
    const Env env;
    const Model model(env);
    build(model, 3, 3);
    add(model, {2, 0}, nullptr);
    BIYOS_model *mdl = model.get();
    const std::vector<BIYOS_index> fun{0, 2};
    const std::vector<BIYOS_index> kept{2, 1};
    ASSERT_EQ(BIYOS_setcbg(mdl, 0, 2, fun.data(), kept.data(), record), BIYOS_OK);
    const std::vector<BIYOS_index> unknown{0, 3};
    EXPECT_EQ(failed(env, BIYOS_setcbg(mdl, 0, 2, fun.data(), unknown.data(), undefined)),
              kBad + "callback 0: Jacobian of function 2: variable 3 is unknown");
    const std::vector<BIYOS_index> uncovered{2, 1};
    EXPECT_EQ(failed(env, BIYOS_setcbg(mdl, 0, 2, uncovered.data(), kept.data(), undefined)),
              kBad + "callback 0: Jacobian function 1 is not covered");
    const BIYOS_index first = 1;
    const BIYOS_index second = 0;
    EXPECT_EQ(failed(env, BIYOS_setcbh(mdl, 0, 1, &first, &second, undefined)),
              kBad + "callback 0: Hessian pair (1, 0) has its second variable below its first");
    EXPECT_EQ(held(model, 0).jacobian_pairs, (std::vector<IndexPair>{{1, 2}, {0, 1}}));
    EXPECT_FALSE(held(model, 0).hessian);
}

TEST(CallbackTest, Expression)
{
    const Env env;
    const Model model(env);
    const Model plain(env);
    build(model, 1, 2);
    build(plain, 1, 2);
    attach_exp(model, 0);
    attach_exp(plain, 0);
    EXPECT_EQ(add(model, {0}, nullptr), 0);
    EXPECT_EQ(add(model, {1}, nullptr), 1);
    attach_exp(model, 1);
    attach_exp(model, 0);
    attach_exp(plain, 1);
    attach_exp(plain, 0);
    EXPECT_EQ(env.message(), "");
    EXPECT_EQ(difference(model.get()->model(), plain.get()->model()), "callback count 2 against 0");
}

TEST(CallbackTest, Shared)
{
    const Env env;
    const Model model(env);
    build(model, 1, 3);
    attach_exp(model, 1);
    EXPECT_EQ(add(model, {1, 0}, nullptr), 0);
    EXPECT_EQ(add(model, {2, 1}, nullptr), 1);
    attach_exp(model, 1);
    EXPECT_EQ(env.message(), "");
    EXPECT_EQ(held(model, 0).covered, (std::vector<Index>{1, 0}));
    EXPECT_EQ(held(model, 1).covered, (std::vector<Index>{2, 1}));
    const std::vector<BIYOS_index> twice{2, 1, 2};
    EXPECT_EQ(
        failed(env,
               BIYOS_addcb(model.get(), 3, twice.data(), BIYOS_DENSE, nullptr, nullptr, nullptr)),
        kBad + "argument 'fun': function 2 is repeated");
    EXPECT_EQ(model.get()->model().n_callbacks(), 2);
}

TEST(CallbackTest, CopyLists)
{
    const Env env;
    const Model source(env);
    cover(source);
    const Model copy(env, source);
    EXPECT_EQ(difference(source.get()->model(), copy.get()->model()), "");
    EXPECT_EQ(held(copy, 1).covered, (std::vector<Index>{1, 2}));
    EXPECT_EQ(held(copy, 2).covered, (std::vector<Index>{3, 1}));
    const Model other(env);
    build(other, 1, 4);
    add(other, {2}, nullptr);
    add(other, {1, 2}, nullptr);
    add(other, {3, 2}, nullptr);
    EXPECT_EQ(difference(copy.get()->model(), other.get()->model()),
              "callback 2: covered function 1: index 1 against 2");
}

TEST(CallbackTest, Deleted)
{
    const Env env;
    const Model model(env);
    cover(model);
    EXPECT_EQ(deleted(env, model, 1), "in use: function 1: used by callback 1");
    EXPECT_EQ(deleted(env, model, 2), "in use: function 2: used by callback 0");
    EXPECT_EQ(deleted(env, model, 3), "in use: function 3: used by callback 2");
    std::vector<BIYOS_index> map(4, 9);
    EXPECT_EQ(BIYOS_delfunrg(model.get(), 0, 1, map.data()), BIYOS_OK);
    EXPECT_EQ(map, (std::vector<BIYOS_index>{-1, 0, 1, 2}));
    EXPECT_EQ(held(model, 0).covered, (std::vector<Index>{1}));
    EXPECT_EQ(held(model, 1).covered, (std::vector<Index>{0, 1}));
    EXPECT_EQ(held(model, 2).covered, (std::vector<Index>{2, 0}));
}

TEST(CallbackTest, DataPointer)
{
    const Env env;
    const Model model(env);
    build(model, 1, 1);
    int tag = 0;
    int other = 0;
    add(model, {0}, &tag);
    EXPECT_EQ(BIYOS_setcbdata(model.get(), 0, &other), BIYOS_OK);
    EXPECT_EQ(data_of(model, 0), &other);
    EXPECT_EQ(BIYOS_setcbdata(model.get(), 0, nullptr), BIYOS_OK);
    EXPECT_EQ(data_of(model, 0), nullptr);
}

TEST(CallbackTest, Locked)
{
    const Env env;
    const Model model(env);
    build(model, 2, 2);
    int tag = 0;
    add(model, {0}, &tag);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    BIYOS_index id = 0;
    const int message = BIYOS_EVTKND_MSG;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), change, &attempt, message, BIYOS_EVTLVL_ERR, &id),
              BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    EXPECT_EQ(attempt.statuses, std::vector<BIYOS_status>(6, BIYOS_ERR_BADARG));
    const std::string locked = kBad + "model: is read-only while delivering an event";
    EXPECT_EQ(attempt.messages, std::vector<std::string>(6, locked));
    EXPECT_EQ(attempt.read, BIYOS_OK);
    EXPECT_EQ(attempt.data, &tag);
    EXPECT_EQ(model.get()->model().n_callbacks(), 1);
    EXPECT_FALSE(held(model, 0).function);
}

TEST(CallbackTest, Busy)
{
    const Env env;
    const Model model(env);
    build(model, 2, 2);
    int tag = 0;
    add(model, {0}, &tag);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    Usage &usage = model.get()->usage();
    ASSERT_TRUE(usage.enter());
    change_all(attempt);
    usage.leave();
    EXPECT_EQ(attempt.statuses, std::vector<BIYOS_status>(6, BIYOS_ERR_BUSY));
    const std::string busy = "busy: model: is in a call on another thread";
    EXPECT_EQ(attempt.messages, std::vector<std::string>(6, busy));
    EXPECT_EQ(attempt.read, BIYOS_OK);
    EXPECT_EQ(attempt.data, &tag);
    EXPECT_EQ(model.get()->model().n_callbacks(), 1);
}

TEST(CallbackTest, Weights)
{
    const Env env;
    const Model model(env);
    build(model, 2, 2);
    Seen seen;
    add(model, {1, 0}, &seen);
    const BIYOS_index first = 0;
    const BIYOS_index second = 1;
    ASSERT_EQ(BIYOS_setcbh(model.get(), 0, 1, &first, &second, record), BIYOS_OK);
    const std::vector<double> point{1.0, 2.0};
    const std::vector<double> weights{3.0, 4.0};
    std::vector<double> out(1);
    const Request request{
        .session = model.get(),
        .callback = 0,
        .thread = 2,
        .x = point,
        .w = weights,
        .v = {},
    };
    const Callback &stored = held(model, 0);
    EXPECT_EQ(stored.hessian(request, out, stored.data), 0);
    EXPECT_EQ(seen.calls, 1);
    EXPECT_EQ(seen.size, sizeof(BIYOS_cbreq));
    EXPECT_EQ(seen.model, model.get());
    EXPECT_EQ(seen.callback, 0);
    EXPECT_EQ(seen.thread, 2);
    EXPECT_EQ(seen.point, point.data());
    EXPECT_EQ(seen.weights, weights.data());
    EXPECT_EQ(seen.direction, nullptr);
    EXPECT_EQ(bits(out[0]), bits(0.5));
}

TEST(CallbackTest, Direction)
{
    const Env env;
    const Model model(env);
    build(model, 2, 1);
    Seen seen;
    add(model, {0}, &seen);
    ASSERT_EQ(BIYOS_setcbhv(model.get(), 0, record), BIYOS_OK);
    const std::vector<double> point{1.0, 2.0};
    const std::vector<double> weights{2.0};
    const std::vector<double> direction{0.0, 1.0};
    std::vector<double> out(2);
    const Request request{
        .session = model.get(),
        .callback = 0,
        .thread = 0,
        .x = point,
        .w = weights,
        .v = direction,
    };
    const Callback &stored = held(model, 0);
    EXPECT_EQ(stored.hessian_product(request, out, stored.data), 0);
    EXPECT_EQ(seen.weights, weights.data());
    EXPECT_EQ(seen.direction, direction.data());
}

TEST(CallbackTest, Returned)
{
    const Env env;
    const Model model(env);
    build(model, 1, 1);
    add(model, {0}, nullptr);
    ASSERT_EQ(BIYOS_setcbf(model.get(), 0, undefined), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcbhv(model.get(), 0, stopped), BIYOS_OK);
    const std::vector<double> point{1.0};
    std::vector<double> out(1);
    const Request request{
        .session = model.get(),
        .callback = 0,
        .thread = 0,
        .x = point,
        .w = {},
        .v = point,
    };
    const Callback &stored = held(model, 0);
    EXPECT_EQ(stored.function(request, out, stored.data), 1);
    EXPECT_EQ(stored.hessian_product(request, out, stored.data), -1);
}

TEST(CallbackTest, CopyShared)
{
    const Env env;
    const Model source(env);
    build(source, 2, 2);
    Seen seen;
    add(source, {1}, &seen);
    const std::vector<BIYOS_index> fun{1, 1};
    const std::vector<BIYOS_index> var{1, 0};
    ASSERT_EQ(BIYOS_setcbf(source.get(), 0, record), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcbg(source.get(), 0, 2, fun.data(), var.data(), record), BIYOS_OK);
    const Model copy(env, source);
    EXPECT_EQ(difference(source.get()->model(), copy.get()->model()), "");
    EXPECT_EQ(data_of(copy, 0), &seen);
    const std::vector<double> point{1.0, 2.0};
    std::vector<double> out(1);
    const Request request{
        .session = copy.get(),
        .callback = 0,
        .thread = 0,
        .x = point,
        .w = {},
        .v = {},
    };
    const Callback &stored = held(copy, 0);
    EXPECT_EQ(stored.function(request, out, stored.data), 0);
    EXPECT_EQ(seen.calls, 1);
    EXPECT_EQ(seen.model, copy.get());
    EXPECT_EQ(seen.weights, nullptr);
    EXPECT_EQ(seen.direction, nullptr);
    int other = 0;
    EXPECT_EQ(BIYOS_setcbdata(copy.get(), 0, &other), BIYOS_OK);
    EXPECT_EQ(data_of(source, 0), &seen);
    EXPECT_EQ(difference(source.get()->model(), copy.get()->model()), "callback 0: data differs");
}

TEST(CallbackTest, Dropped)
{
    const Env env;
    const Model model(env);
    cover(model);
    int tag = 0;
    ASSERT_EQ(BIYOS_setcbdata(model.get(), 2, &tag), BIYOS_OK);
    std::vector<BIYOS_index> map(3, 9);
    const std::vector<BIYOS_index> idx{1};
    EXPECT_EQ(BIYOS_delcbsp(model.get(), 1, idx.data(), map.data()), BIYOS_OK);
    EXPECT_EQ(map, (std::vector<BIYOS_index>{0, -1, 1}));
    EXPECT_EQ(covered_by(model), (std::vector<BIYOS_index>{2, 3}));
    EXPECT_EQ(data_of(model, 1), &tag);
    EXPECT_EQ(count_of(model, 1), 2);
    const BIYOS_index function = 2;
    EXPECT_EQ(in_use(env, BIYOS_delfunsp(model.get(), 1, &function, nullptr)),
              "in use: function 2: used by callback 0");
    EXPECT_EQ(BIYOS_delcbrg(model.get(), 0, 1, map.data()), BIYOS_OK);
    EXPECT_EQ(map[0], -1);
    EXPECT_EQ(map[1], 0);
    EXPECT_EQ(BIYOS_delfunsp(model.get(), 1, &function, nullptr), BIYOS_OK);
    EXPECT_EQ(held(model, 0).covered, (std::vector<Index>{2, 1}));
}

TEST(CallbackTest, DropNull)
{
    const Env env;
    const Model model(env);
    cover(model);
    EXPECT_EQ(BIYOS_delcbsp(model.get(), 0, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_delcbrg(model.get(), 3, 3, nullptr), BIYOS_OK);
    EXPECT_EQ(model.get()->model().n_callbacks(), 3);
    EXPECT_EQ(BIYOS_delcbrg(model.get(), 0, 3, nullptr), BIYOS_OK);
    EXPECT_EQ(model.get()->model().n_callbacks(), 0);
    EXPECT_EQ(BIYOS_delvarsp(model.get(), 1, std::vector<BIYOS_index>{0}.data(), nullptr),
              BIYOS_OK);
}

TEST(CallbackTest, DropRefused)
{
    const Env env;
    const Model model(env);
    cover(model);
    EXPECT_EQ(drop_refusal(env, model, {3}), kBad + "callback 3: unknown");
    EXPECT_EQ(drop_refusal(env, model, {0, -1}), kBad + "callback -1: unknown");
    EXPECT_EQ(drop_refusal(env, model, {2, 0, 2}), kBad + "argument 'idx': callback 2 is repeated");
    EXPECT_EQ(failed(env, BIYOS_delcbsp(model.get(), -1, nullptr, nullptr)),
              kBad + "argument 'n': -1 is negative");
    EXPECT_EQ(failed(env, BIYOS_delcbsp(model.get(), 1, nullptr, nullptr)),
              kBad + "argument 'idx': is null");
    EXPECT_EQ(range_refusal(env, model, 1, 4), kBad + "callback 3: unknown");
    EXPECT_EQ(range_refusal(env, model, 2, 1), kBad + "argument 'end': 1 is below 'beg' 2");
    EXPECT_EQ(range_refusal(env, model, -1, 1), kBad + "callback -1: unknown");
    EXPECT_EQ(covered_by(model), (std::vector<BIYOS_index>{2, 1, 3}));
}

TEST(CallbackTest, DropCopied)
{
    const Env env;
    const Model source(env);
    cover(source);
    const Model copy(env, source);
    ASSERT_EQ(BIYOS_delcbrg(source.get(), 0, 2, nullptr), BIYOS_OK);
    EXPECT_EQ(covered_by(source), (std::vector<BIYOS_index>{3}));
    EXPECT_EQ(covered_by(copy), (std::vector<BIYOS_index>{2, 1, 3}));
    EXPECT_EQ(count_of(copy, 1), 2);
}
