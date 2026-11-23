#include "biyos/biyos.h"
#include "doubles.hpp"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::expect_doubles;
using biyos::test::Model;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

struct Columns
{
    std::vector<int> kinds;
    std::vector<double> lower;
    std::vector<double> upper;
};

Columns read_all(const Model &model)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnvar(model.get(), &n), BIYOS_OK);
    const auto size = static_cast<std::size_t>(n);
    Columns read{.kinds = std::vector<int>(size),
                 .lower = std::vector<double>(size),
                 .upper = std::vector<double>(size)};
    const BIYOS_status status =
        BIYOS_getvarrg(model.get(), 0, n, read.kinds.data(), read.lower.data(), read.upper.data());
    EXPECT_EQ(status, BIYOS_OK);
    return read;
}

void expect_columns(const Columns &found, const Columns &wanted)
{
    EXPECT_EQ(found.kinds, wanted.kinds);
    expect_doubles(found.lower, wanted.lower);
    expect_doubles(found.upper, wanted.upper);
}

const Columns kColumns{
    .kinds = {BIYOS_VAR_CONT, BIYOS_VAR_BIN, BIYOS_VAR_INT, BIYOS_VAR_SEMICONT, BIYOS_VAR_SEMIINT},
    .lower = {-kInfinity, 0.0, -3.0, 2.0, 1.0},
    .upper = {kInfinity, 1.0, 7.0, 10.0, 4.0},
};

void add_columns(const Model &model)
{
    const BIYOS_int *kinds = kColumns.kinds.data();
    const double *lower = kColumns.lower.data();
    const double *upper = kColumns.upper.data();
    ASSERT_EQ(BIYOS_addvar(model.get(), 5, kinds, lower, upper, nullptr), BIYOS_OK);
}

Columns read_listed(const Model &model, const std::vector<BIYOS_index> &indices)
{
    const auto n = static_cast<BIYOS_index>(indices.size());
    Columns read{
        .kinds = std::vector<int>(indices.size(), -1),
        .lower = std::vector<double>(indices.size(), 9.0),
        .upper = std::vector<double>(indices.size(), 9.0),
    };
    const BIYOS_status status =
        BIYOS_getvarbndsp(model.get(), n, indices.data(), read.lower.data(), read.upper.data());
    EXPECT_EQ(status, BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarkndsp(model.get(), n, indices.data(), read.kinds.data()), BIYOS_OK);
    return read;
}

struct Attempt
{
    BIYOS_env *env = nullptr;
    BIYOS_model *model = nullptr;
    std::vector<BIYOS_status> statuses;
    std::vector<std::string> messages;
    int count = -1;
};

void note(Attempt &attempt, BIYOS_status status)
{
    attempt.statuses.push_back(status);
    attempt.messages.emplace_back(biyos::test::last_message(attempt.env));
}

void change(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_model *model = attempt.model;
    BIYOS_model *made = nullptr;
    const BIYOS_index index = 0;
    const BIYOS_real bound = 0.0;
    const BIYOS_int kind = BIYOS_VAR_INT;
    note(attempt, BIYOS_newmdl(attempt.env, &made));
    note(attempt, BIYOS_freemdl(model));
    note(attempt, BIYOS_addvar(model, 1, nullptr, nullptr, nullptr, nullptr));
    note(attempt, BIYOS_setvarbndsp(model, 1, &index, &bound, &bound));
    note(attempt, BIYOS_setvarkndsp(model, 1, &index, &kind));
    note(attempt, BIYOS_getvarbndsp(model, 1, &index, nullptr, nullptr));
    note(attempt, BIYOS_getvarkndsp(model, 1, &index, nullptr));
    note(attempt, BIYOS_getnvar(model, &attempt.count));
    note(attempt, BIYOS_getvarrg(model, 0, 1, nullptr, nullptr, nullptr));
}

void keep(const BIYOS_event *event, void *data)
{
    static_cast<std::vector<std::string> *>(data)->emplace_back(BIYOS_getevttext(event));
}

void fail(const Env &env)
{
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
}

}

TEST(CApiTest, ModelCreate)
{
    const Env env;
    BIYOS_model *model = nullptr;
    ASSERT_EQ(BIYOS_newmdl(env.get(), &model), BIYOS_OK);
    EXPECT_NE(model, nullptr);
    EXPECT_EQ(env.message(), "");
    EXPECT_EQ(BIYOS_freemdl(model), BIYOS_OK);
}

TEST(CApiTest, NullModel)
{
    const Env env;
    const Model kept(env);
    BIYOS_model *model = kept.get();
    EXPECT_EQ(BIYOS_newmdl(nullptr, &model), BIYOS_ERR_BADARG);
    EXPECT_EQ(model, nullptr);
    EXPECT_EQ(BIYOS_newmdl(env.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'mdl': is null");
    EXPECT_EQ(BIYOS_freemdl(nullptr), BIYOS_OK);
    const BIYOS_index index = 0;
    const BIYOS_real bound = 0.0;
    BIYOS_index n = 0;
    EXPECT_EQ(BIYOS_addvar(nullptr, 1, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setvarbndsp(nullptr, 1, &index, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getvarbndsp(nullptr, 1, &index, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setvarkndsp(nullptr, 1, &index, &index), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getvarkndsp(nullptr, 1, &index, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnvar(nullptr, &n), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getvarrg(nullptr, 0, 0, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    BIYOS_offset needed = 0;
    EXPECT_EQ(BIYOS_addfun(nullptr, 0, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnfun(nullptr, &n), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getcstrg(nullptr, 0, 0, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addlintermsp(nullptr, 0, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addrlintermrg(nullptr, 0, 0, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addclintermrg(nullptr, 0, 0, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnrlintermsp(nullptr, 1, &index, &needed), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnrlintermrg(nullptr, 0, 0, &needed), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getrlintermsp(nullptr, 1, &index, 0, &needed, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getrlintermrg(nullptr, 0, 0, 0, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnclintermsp(nullptr, 1, &index, &needed), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnclintermrg(nullptr, 0, 0, &needed), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getclintermsp(nullptr, 1, &index, 0, &needed, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getclintermrg(nullptr, 0, 0, 0, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addquadtermsp(nullptr, 0, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addquadtermrg(nullptr, 0, 0, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnquadtermsp(nullptr, 1, &index, &needed), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnquadtermrg(nullptr, 0, 0, &needed), BIYOS_ERR_BADARG);
    EXPECT_EQ(
        BIYOS_getquadtermsp(nullptr, 1, &index, 0, &needed, nullptr, nullptr, nullptr, nullptr),
        BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getquadtermrg(nullptr, 0, 0, 0, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addcon(nullptr, 1, &index, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setconbndsp(nullptr, 1, &index, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getconbndsp(nullptr, 1, &index, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getncon(nullptr, &n), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getconrg(nullptr, 0, 0, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addobj(nullptr, 1, &index, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setobjsensesp(nullptr, 1, &index, &index), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getobjsensesp(nullptr, 1, &index, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setobjpriosp(nullptr, 1, &index, &index), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getobjpriosp(nullptr, 1, &index, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setobjcoefsp(nullptr, 1, &index, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getobjcoefsp(nullptr, 1, &index, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setobjtolsp(nullptr, 1, &index, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getobjtolsp(nullptr, 1, &index, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnobj(nullptr, &n), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getobjrg(nullptr, 0, 0, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addccon(nullptr, 1, &index, &index, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setcconbndsp(nullptr, 1, &index, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getcconbndsp(nullptr, 1, &index, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setccontolsp(nullptr, 1, &index, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getccontolsp(nullptr, 1, &index, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_delccontolsp(nullptr, 1, &index), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnccon(nullptr, &n), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getcconrg(nullptr, 0, 0, nullptr, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    BIYOS_offset node = 0;
    EXPECT_EQ(BIYOS_addvarnode(nullptr, 0, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addnode(nullptr, BIYOS_OP_EXP, 1, &node, nullptr, &node), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addexpr(nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_attachexprsp(nullptr, 0, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_attachexprrg(nullptr, 0, 0, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_clrfunsp(nullptr, 0, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_clrfunrg(nullptr, 0, 0), BIYOS_ERR_BADARG);
    BIYOS_offset nnz = 0;
    EXPECT_EQ(BIYOS_getnexprsp(nullptr, 0, nullptr, &nnz, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getnexprrg(nullptr, 0, 0, &nnz, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getexprsp(nullptr, 0, nullptr, 0, 0, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getexprrg(nullptr, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_addcb(nullptr, 1, &index, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setcbf(nullptr, 0, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setcbg(nullptr, 0, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setcbh(nullptr, 0, 0, nullptr, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setcbhv(nullptr, 0, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setcbdata(nullptr, 0, nullptr), BIYOS_ERR_BADARG);
    void *data = nullptr;
    EXPECT_EQ(BIYOS_getcbdata(nullptr, 0, &data), BIYOS_ERR_BADARG);
}

TEST(CApiTest, HeldFree)
{
    BIYOS_env *env = nullptr;
    BIYOS_model *first = nullptr;
    BIYOS_model *second = nullptr;
    ASSERT_EQ(BIYOS_newenv(&env), BIYOS_OK);
    ASSERT_EQ(BIYOS_newmdl(env, &first), BIYOS_OK);
    ASSERT_EQ(BIYOS_newmdl(env, &second), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(env), BIYOS_ERR_BADARG);
    EXPECT_STREQ(biyos::test::last_message(env),
                 "invalid argument: environment: has a model that is not freed");
    EXPECT_EQ(BIYOS_freemdl(first), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(env), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_setenvintopt(env, BIYOS_OPT_SEED, 5), BIYOS_OK);
    EXPECT_EQ(BIYOS_freemdl(second), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(env), BIYOS_OK);
}

TEST(CApiTest, LockedModel)
{
    const Env env;
    const Model model(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    ASSERT_EQ(BIYOS_addvar(model.get(), 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), change, &attempt, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    fail(env);
    const BIYOS_status refused = BIYOS_ERR_BADARG;
    const std::vector<BIYOS_status> statuses{
        refused,
        refused,
        refused,
        refused,
        refused,
        BIYOS_OK,
        BIYOS_OK,
        BIYOS_OK,
        BIYOS_OK,
    };
    const std::string locked = "invalid argument: model: is read-only while delivering an event";
    const std::string failure = "invalid argument: option 'seed': -1 is outside [0, 2147483647]";
    const std::vector<std::string> messages{
        "invalid argument: environment: is read-only while delivering an event",
        locked,
        locked,
        locked,
        locked,
        locked,
        locked,
        locked,
        locked,
    };
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages, messages);
    EXPECT_EQ(attempt.count, 1);
    EXPECT_EQ(env.message(), failure);
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
}

TEST(CApiTest, BulkAdd)
{
    const Env env;
    const Model model(env);
    const Columns added{
        .kinds =
            {BIYOS_VAR_CONT, BIYOS_VAR_BIN, BIYOS_VAR_INT, BIYOS_VAR_SEMICONT, BIYOS_VAR_SEMIINT},
        .lower = {-kInfinity, 0.0, -3.0, 2.0, 1.0},
        .upper = {kInfinity, 1.0, 7.0, 10.0, 4.0},
    };
    const int *kinds = added.kinds.data();
    const double *low = added.lower.data();
    const double *high = added.upper.data();
    ASSERT_EQ(BIYOS_addvar(model.get(), 5, kinds, low, high, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvar(model.get(), 0, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    expect_columns(read_all(model), added);
    BIYOS_int kind = 0;
    BIYOS_real lower = 0.0;
    BIYOS_real upper = 0.0;
    EXPECT_EQ(BIYOS_getvarrg(model.get(), 2, 1, &kind, &lower, &upper), BIYOS_OK);
    EXPECT_EQ(kind, BIYOS_VAR_INT);
    EXPECT_DOUBLE_EQ(lower, -3.0);
    EXPECT_DOUBLE_EQ(upper, 7.0);
    EXPECT_EQ(BIYOS_getvarrg(model.get(), 5, 0, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, FirstIndex)
{
    const Env env;
    const Model model(env);
    BIYOS_index start = -1;
    ASSERT_EQ(BIYOS_addvar(model.get(), 0, nullptr, nullptr, nullptr, &start), BIYOS_OK);
    EXPECT_EQ(start, 0);
    ASSERT_EQ(BIYOS_addvar(model.get(), 1, nullptr, nullptr, nullptr, &start), BIYOS_OK);
    EXPECT_EQ(start, 0);
    ASSERT_EQ(BIYOS_addvar(model.get(), 3, nullptr, nullptr, nullptr, &start), BIYOS_OK);
    EXPECT_EQ(start, 1);
    ASSERT_EQ(BIYOS_addvar(model.get(), 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvar(model.get(), 0, nullptr, nullptr, nullptr, &start), BIYOS_OK);
    EXPECT_EQ(start, 6);
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, RefusedAdd)
{
    const Env env;
    const Model model(env);
    BIYOS_index start = -1;
    const int kinds[] = {BIYOS_VAR_CONT, 9};
    EXPECT_EQ(BIYOS_addvar(model.get(), 2, kinds, nullptr, nullptr, &start), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 1: kind 9 is unknown");
    const double lower[] = {0.0, kInfinity};
    EXPECT_EQ(BIYOS_addvar(model.get(), 2, nullptr, lower, nullptr, &start), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 1: lower bound is infinity");
    EXPECT_EQ(BIYOS_addvar(model.get(), -1, nullptr, nullptr, nullptr, &start), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(start, -1);
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnvar(model.get(), &n), BIYOS_OK);
    EXPECT_EQ(n, 0);
}

TEST(CApiTest, BinaryKind)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addvar(model.get(), 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const BIYOS_index index = 0;
    const BIYOS_int binary = BIYOS_VAR_BIN;
    EXPECT_EQ(BIYOS_setvarkndsp(model.get(), 1, &index, &binary), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(),
              "invalid argument: variable 0: bounds [-inf, inf] are outside "
              "[0, 1], the widest of its kind");
    const BIYOS_real lower = 0.0;
    const BIYOS_real upper = 1.0;
    const BIYOS_real wide = 2.0;
    ASSERT_EQ(BIYOS_setvarbndsp(model.get(), 1, &index, &lower, &upper), BIYOS_OK);
    EXPECT_EQ(BIYOS_setvarkndsp(model.get(), 1, &index, &binary), BIYOS_OK);
    EXPECT_EQ(BIYOS_setvarbndsp(model.get(), 1, &index, nullptr, &wide), BIYOS_ERR_BADARG);
    expect_columns(read_all(model), Columns{.kinds = {binary}, .lower = {0.0}, .upper = {1.0}});
}

TEST(CApiTest, BadIndex)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addvar(model.get(), 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const Columns before = read_all(model);
    const int beyond[] = {0, 3};
    const double bounds[] = {0.0, 1.0};
    const int kinds[] = {BIYOS_VAR_INT, BIYOS_VAR_INT};
    EXPECT_EQ(BIYOS_setvarbndsp(model.get(), 2, beyond, bounds, bounds), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 3: unknown");
    const int negative[] = {0, -1};
    EXPECT_EQ(BIYOS_setvarkndsp(model.get(), 2, negative, kinds), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable -1: unknown");
    expect_columns(read_all(model), before);
    int found[] = {0, 0};
    EXPECT_EQ(BIYOS_getvarrg(model.get(), 2, 2, found, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 3: unknown");
    EXPECT_EQ(BIYOS_getvarrg(model.get(), -1, 1, found, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable -1: unknown");
    EXPECT_EQ(BIYOS_getvarrg(model.get(), 0, -1, found, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(found[0], 0);
    EXPECT_EQ(found[1], 0);
}

TEST(CApiTest, NullAdd)
{
    const Env env;
    const Model model(env);
    const int kinds[] = {BIYOS_VAR_BIN, BIYOS_VAR_INT};
    const double bounds[] = {0.5, 0.75};
    ASSERT_EQ(BIYOS_addvar(model.get(), 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvar(model.get(), 2, kinds, nullptr, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvar(model.get(), 2, kinds, bounds, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addvar(model.get(), 2, kinds, nullptr, bounds, nullptr), BIYOS_OK);
    const int continuous = BIYOS_VAR_CONT;
    const Columns wanted{
        .kinds =
            {continuous, continuous, kinds[0], kinds[1], kinds[0], kinds[1], kinds[0], kinds[1]},
        .lower = {-kInfinity, -kInfinity, 0.0, -kInfinity, 0.5, 0.75, 0.0, -kInfinity},
        .upper = {kInfinity, kInfinity, 1.0, kInfinity, 1.0, kInfinity, 0.5, 0.75},
    };
    expect_columns(read_all(model), wanted);
}

TEST(CApiTest, NullSet)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addvar(model.get(), 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const int indices[] = {1, 0};
    const double lower[] = {1.0, 2.0};
    const double upper[] = {3.0, 4.0};
    const int kinds[] = {BIYOS_VAR_INT, BIYOS_VAR_SEMIINT};
    ASSERT_EQ(BIYOS_setvarbndsp(model.get(), 2, indices, lower, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvarbndsp(model.get(), 1, indices, nullptr, upper), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvarbndsp(model.get(), 2, indices, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvarkndsp(model.get(), 2, indices, nullptr), BIYOS_OK);
    const int continuous = BIYOS_VAR_CONT;
    const Columns bounded{.kinds = {continuous, continuous},
                          .lower = {2.0, 1.0},
                          .upper = {kInfinity, 3.0}};
    expect_columns(read_all(model), bounded);
    ASSERT_EQ(BIYOS_setvarkndsp(model.get(), 2, indices, kinds), BIYOS_OK);
    expect_columns(
        read_all(model),
        Columns{.kinds = {kinds[1], kinds[0]}, .lower = bounded.lower, .upper = bounded.upper});
    EXPECT_EQ(BIYOS_setvarbndsp(model.get(), 2, nullptr, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_setvarkndsp(model.get(), 2, nullptr, kinds), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_setvarbndsp(model.get(), 0, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_setvarkndsp(model.get(), 0, nullptr, nullptr), BIYOS_OK);
}

TEST(CApiTest, NullGet)
{
    const Env env;
    const Model model(env);
    const int kinds[] = {BIYOS_VAR_INT, BIYOS_VAR_BIN};
    const double lower[] = {-1.0, 0.0};
    const double upper[] = {1.0, 0.5};
    ASSERT_EQ(BIYOS_addvar(model.get(), 2, kinds, lower, upper, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarrg(model.get(), 0, 2, nullptr, nullptr, nullptr), BIYOS_OK);
    int found[] = {0, 0};
    double low[] = {9.0, 9.0};
    double high[] = {9.0, 9.0};
    EXPECT_EQ(BIYOS_getvarrg(model.get(), 0, 2, found, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarrg(model.get(), 0, 2, nullptr, low, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarrg(model.get(), 1, 1, nullptr, nullptr, high), BIYOS_OK);
    EXPECT_EQ(found[1], BIYOS_VAR_BIN);
    EXPECT_DOUBLE_EQ(low[0], -1.0);
    EXPECT_DOUBLE_EQ(high[0], 0.5);
    EXPECT_DOUBLE_EQ(high[1], 9.0);
    EXPECT_EQ(BIYOS_getnvar(model.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': is null");
}

TEST(CApiTest, ListedVariables)
{
    const Env env;
    const Model model(env);
    add_columns(model);
    const Columns wanted{
        .kinds = {BIYOS_VAR_SEMIINT, BIYOS_VAR_CONT, BIYOS_VAR_SEMIINT, BIYOS_VAR_INT},
        .lower = {1.0, -kInfinity, 1.0, -3.0},
        .upper = {4.0, kInfinity, 4.0, 7.0},
    };
    expect_columns(read_listed(model, {4, 0, 4, 2}), wanted);
    expect_columns(read_listed(model, {}), Columns{});
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, VariableSetters)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addvar(model.get(), 3, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    const BIYOS_index indices[] = {2, 0, 2};
    const double lower[] = {0.0, -1.0, 0.5};
    const double upper[] = {1.0, 2.0, 1.0};
    const BIYOS_int kinds[] = {BIYOS_VAR_INT, BIYOS_VAR_SEMICONT, BIYOS_VAR_BIN};
    ASSERT_EQ(BIYOS_setvarbndsp(model.get(), 3, indices, lower, upper), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvarkndsp(model.get(), 3, indices, kinds), BIYOS_OK);
    std::vector<double> read_lower(3, 9.0);
    std::vector<double> read_upper(3, 9.0);
    std::vector<BIYOS_int> found(3, -1);
    ASSERT_EQ(BIYOS_getvarbndsp(model.get(), 3, indices, read_lower.data(), read_upper.data()),
              BIYOS_OK);
    ASSERT_EQ(BIYOS_getvarkndsp(model.get(), 3, indices, found.data()), BIYOS_OK);
    const Columns wanted{
        .kinds = {BIYOS_VAR_BIN, BIYOS_VAR_SEMICONT, BIYOS_VAR_BIN},
        .lower = {0.5, -1.0, 0.5},
        .upper = {1.0, 2.0, 1.0},
    };
    expect_columns(Columns{.kinds = found, .lower = read_lower, .upper = read_upper}, wanted);
    const Columns untouched{.kinds = {BIYOS_VAR_CONT}, .lower = {-kInfinity}, .upper = {kInfinity}};
    expect_columns(read_listed(model, {1}), untouched);
}

TEST(CApiTest, VariableNulls)
{
    const Env env;
    const Model model(env);
    add_columns(model);
    const BIYOS_index indices[] = {3, 1};
    double lower[] = {9.0, 9.0};
    double upper[] = {9.0, 9.0};
    BIYOS_int kinds[] = {-1, -1};
    const BIYOS_model *held = model.get();
    EXPECT_EQ(BIYOS_getvarbndsp(held, 2, indices, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarkndsp(held, 2, indices, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarbndsp(held, 2, indices, lower, nullptr), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarbndsp(held, 1, indices, nullptr, upper), BIYOS_OK);
    expect_doubles({lower[0], lower[1]}, {2.0, 0.0});
    expect_doubles({upper[0], upper[1]}, {10.0, 9.0});
    EXPECT_EQ(BIYOS_getvarbndsp(held, 0, nullptr, lower, upper), BIYOS_OK);
    EXPECT_EQ(BIYOS_getvarkndsp(held, 0, nullptr, kinds), BIYOS_OK);
    EXPECT_EQ(std::vector<BIYOS_int>({kinds[0], kinds[1]}), std::vector<BIYOS_int>({-1, -1}));
    EXPECT_EQ(env.message(), "");
}

TEST(CApiTest, VariableRefusals)
{
    const Env env;
    const Model model(env);
    add_columns(model);
    const BIYOS_model *held = model.get();
    const BIYOS_index beyond[] = {0, 5};
    const BIYOS_index negative[] = {1, -1};
    double lower[] = {9.0, 9.0};
    double upper[] = {9.0, 9.0};
    BIYOS_int kinds[] = {-1, -1};
    EXPECT_EQ(BIYOS_getvarbndsp(held, 2, beyond, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 5: unknown");
    EXPECT_EQ(BIYOS_getvarkndsp(held, 2, beyond, kinds), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable 5: unknown");
    EXPECT_EQ(BIYOS_getvarbndsp(held, 2, negative, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable -1: unknown");
    EXPECT_EQ(BIYOS_getvarkndsp(held, 2, negative, kinds), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: variable -1: unknown");
    EXPECT_EQ(BIYOS_getvarbndsp(held, -1, beyond, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(BIYOS_getvarkndsp(held, -1, beyond, kinds), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    EXPECT_EQ(BIYOS_getvarbndsp(held, 1, nullptr, lower, upper), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    EXPECT_EQ(BIYOS_getvarkndsp(held, 1, nullptr, kinds), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'idx': is null");
    expect_doubles({lower[0], lower[1]}, {9.0, 9.0});
    expect_doubles({upper[0], upper[1]}, {9.0, 9.0});
    EXPECT_EQ(std::vector<BIYOS_int>({kinds[0], kinds[1]}), std::vector<BIYOS_int>({-1, -1}));
}

TEST(CApiTest, ModelEvent)
{
    const Env env;
    const Model model(env);
    std::vector<std::string> texts;
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), keep, &texts, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    const BIYOS_index index = 0;
    const BIYOS_real bound = 0.0;
    EXPECT_EQ(BIYOS_setvarbndsp(model.get(), 1, &index, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(texts, std::vector<std::string>{"invalid argument: variable 0: unknown"});
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
}
