#include "biyos/biyos.h"
#include "doubles.hpp"
#include "handles.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::expect_doubles;
using biyos::test::Model;

constexpr double kNoConstant = -9.5;

int n_functions(const Model &model)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnfun(model.get(), &n), BIYOS_OK);
    return n;
}

struct Attempt
{
    BIYOS_env *env = nullptr;
    BIYOS_model *model = nullptr;
    std::vector<BIYOS_status> statuses;
    std::vector<std::string> messages;
};

void note(Attempt &attempt, BIYOS_status status)
{
    attempt.statuses.push_back(status);
    attempt.messages.emplace_back(BIYOS_lasterror(attempt.env));
}

void change(const BIYOS_event *, void *data)
{
    Attempt &attempt = *static_cast<Attempt *>(data);
    BIYOS_model *model = attempt.model;
    BIYOS_index n = 0;
    note(attempt, BIYOS_addfun(model, 1, nullptr));
    note(attempt, BIYOS_getnfun(model, &n));
}

}

TEST(FunctionTest, Empty)
{
    const Env env;
    const Model model(env);
    ASSERT_EQ(BIYOS_addvar(model.get(), 2, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    BIYOS_index start = -1;
    ASSERT_EQ(BIYOS_addfun(model.get(), 0, &start), BIYOS_OK);
    EXPECT_EQ(start, 0);
    ASSERT_EQ(BIYOS_addfun(model.get(), 2, &start), BIYOS_OK);
    EXPECT_EQ(start, 0);
    ASSERT_EQ(BIYOS_addfun(model.get(), 3, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model.get(), 1, &start), BIYOS_OK);
    EXPECT_EQ(start, 5);
    EXPECT_EQ(n_functions(model), 6);
    std::vector<double> constants(6, kNoConstant);
    ASSERT_EQ(BIYOS_getcstrg(model.get(), 0, 6, constants.data()), BIYOS_OK);
    expect_doubles(constants, std::vector<double>(6, 0.0));
    BIYOS_offset nnz = -1;
    ASSERT_EQ(BIYOS_getnrlintermrg(model.get(), 0, 6, &nnz), BIYOS_OK);
    EXPECT_EQ(nnz, 0);
    ASSERT_EQ(BIYOS_getnquadtermrg(model.get(), 0, 6, &nnz), BIYOS_OK);
    EXPECT_EQ(nnz, 0);
}

TEST(FunctionTest, Refusals)
{
    const Env env;
    const Model model(env);
    BIYOS_index start = -1;
    ASSERT_EQ(BIYOS_addfun(model.get(), 1, &start), BIYOS_OK);
    EXPECT_EQ(BIYOS_addfun(model.get(), -1, &start), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': -1 is negative");
    const BIYOS_index most = (std::numeric_limits<BIYOS_index>::max)();
    EXPECT_EQ(BIYOS_addfun(model.get(), most, &start), BIYOS_ERR_NOMEMORY);
    EXPECT_EQ(env.message(), "out of memory: model: no function index left");
    EXPECT_EQ(start, 0);
    EXPECT_EQ(n_functions(model), 1);
    EXPECT_EQ(BIYOS_getnfun(model.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'n': is null");
}

TEST(FunctionTest, Locked)
{
    const Env env;
    const Model model(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    ASSERT_EQ(BIYOS_addfun(model.get(), 1, nullptr), BIYOS_OK);
    BIYOS_index id = 0;
    const int message = BIYOS_EVT_MESSAGE;
    ASSERT_EQ(BIYOS_addenvhook(env.get(), change, &attempt, message, BIYOS_LVL_ERR, &id), BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(env.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const std::vector<BIYOS_status> statuses{BIYOS_ERR_BADARG, BIYOS_OK};
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages[0],
              "invalid argument: model: is read-only while delivering an event");
    EXPECT_EQ(BIYOS_delenvhook(env.get(), id), BIYOS_OK);
    EXPECT_EQ(n_functions(model), 1);
}
