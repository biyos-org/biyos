#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "handles.hpp"
#include "model/compare/compare.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{

using biyos::difference;
using biyos::test::Env;
using biyos::test::last_message;
using biyos::test::Model;

const std::string kLocked =
    "invalid argument: argument 'env': is read-only while delivering an event";

struct Held
{
    BIYOS_env *env = nullptr;
    BIYOS_model *model = nullptr;
};

struct Delivery
{
    Held busy;
    Held idle;
    std::vector<BIYOS_status> statuses;
    std::vector<bool> nulls;
};

void add_expressions(BIYOS_model *model, const BIYOS_index *functions)
{
    BIYOS_offset x = -1;
    BIYOS_offset sine = -1;
    ASSERT_EQ(BIYOS_addvarnode(model, 1, &x), BIYOS_OK);
    ASSERT_EQ(BIYOS_addnode(model, BIYOS_OP_SIN, 1, &x, nullptr, &sine), BIYOS_OK);
    const std::int64_t counts[] = {3};
    const int code[] = {BIYOS_OP_NODE, BIYOS_OP_VAR, BIYOS_OP_PROD};
    const std::int64_t argument[] = {sine, 2, 2};
    const double values[] = {2.0};
    BIYOS_offset product = -1;
    ASSERT_EQ(BIYOS_addexpr(model, 1, counts, code, argument, values, &product), BIYOS_OK);
    const BIYOS_offset roots[] = {product, sine};
    ASSERT_EQ(BIYOS_attachexprsp(model, 2, functions, roots), BIYOS_OK);
}

void add_sample(BIYOS_model *model)
{
    const int kinds[] = {BIYOS_VARKND_CONT, BIYOS_VARKND_BIN, BIYOS_VARKND_INT};
    const double lower[] = {-0.0, 0.0, -1.0};
    const double upper[] = {2.5, 1.0, 3.0};
    const double constants[] = {1.5, 0.1};
    const std::int64_t counts[] = {2, 1};
    const int variables[] = {2, 0, 1};
    const double values[] = {-1.0, 2.0, 0.1};
    const int owners[] = {1, 0, 0};
    const int rows[] = {2, 0, 1};
    const int columns[] = {2, 1, 1};
    const double products[] = {0.5, -1.0, 3.0};
    const int functions[] = {0, 1, 0};
    const int targets[] = {1, 0};
    const int senses[] = {BIYOS_OBJSENSE_MAX, BIYOS_OBJSENSE_MIN};
    const int priorities[] = {3, -2};
    const double weights[] = {0.5, 2.0};
    const double absolute[] = {0.25, 0.0};
    const double relative[] = {0.0, 0.125};
    const BIYOS_index added[] = {0, 1};
    const int switches[] = {1, 1};
    const int active[] = {1, 0};
    const double floor[] = {-1.0, 0.0};
    ASSERT_EQ(BIYOS_addvar(model, 3, kinds, lower, upper, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvartolsp(model, 2, added, absolute, relative), BIYOS_OK);
    ASSERT_EQ(BIYOS_addfun(model, 2, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcstrg(model, 0, 2, constants), BIYOS_OK);
    ASSERT_EQ(BIYOS_addrlintermrg(model, 0, 2, counts, variables, values), BIYOS_OK);
    ASSERT_EQ(BIYOS_addquadterm(model, 3, owners, rows, columns, products), BIYOS_OK);
    add_expressions(model, added);
    ASSERT_EQ(BIYOS_addcon(model, 3, functions, lower, upper, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcontolsp(model, 1, added + 1, absolute, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addobj(model, 2, targets, senses, priorities, weights, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setobjtolsp(model, 2, added, absolute, relative), BIYOS_OK);
    ASSERT_EQ(BIYOS_addind(model, 2, switches, active, targets, lower, upper, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_addccon(model, 2, targets, functions, floor, nullptr, nullptr), BIYOS_OK);
    ASSERT_EQ(BIYOS_setccontolsp(model, 1, added + 1, nullptr, relative + 1), BIYOS_OK);
    const BIYOS_index named[] = {2, 0};
    const BIYOS_index first[] = {1};
    const char *names[] = {"z", "x"};
    ASSERT_EQ(BIYOS_setmdlname(model, "sample"), BIYOS_OK);
    ASSERT_EQ(BIYOS_setvarnamesp(model, 2, named, names), BIYOS_OK);
    ASSERT_EQ(BIYOS_setfunnamesp(model, 1, first, names), BIYOS_OK);
    ASSERT_EQ(BIYOS_setconnamesp(model, 2, named, names), BIYOS_OK);
    ASSERT_EQ(BIYOS_setobjnamesp(model, 1, first, names + 1), BIYOS_OK);
    ASSERT_EQ(BIYOS_setindnamesp(model, 1, first, names), BIYOS_OK);
    ASSERT_EQ(BIYOS_setcconnamesp(model, 1, first, names), BIYOS_OK);
}

BIYOS_env *created()
{
    BIYOS_env *env = nullptr;
    EXPECT_EQ(BIYOS_newenv(&env), BIYOS_OK);
    return env;
}

Held made_on(BIYOS_env *env)
{
    BIYOS_model *model = nullptr;
    EXPECT_EQ(BIYOS_newmdl(env, &model), BIYOS_OK);
    add_sample(model);
    return Held{.env = env, .model = model};
}

Held copied(const Held &source, BIYOS_env *env)
{
    BIYOS_model *copy = nullptr;
    EXPECT_EQ(BIYOS_copymdl(source.model, env, &copy), BIYOS_OK);
    EXPECT_NE(copy, nullptr);
    EXPECT_EQ(difference(source.model->model(), copy->model()), "");
    return Held{.env = env, .model = copy};
}

int n_variables(BIYOS_model *model)
{
    BIYOS_index n = -1;
    EXPECT_EQ(BIYOS_getnvar(model, &n), BIYOS_OK);
    return n;
}

void free_both(const Held &first, const Held &second)
{
    EXPECT_EQ(BIYOS_freemdl(first.model), BIYOS_OK);
    if (first.env != second.env)
    {
        EXPECT_EQ(BIYOS_freeenv(first.env), BIYOS_OK);
    }
    EXPECT_EQ(BIYOS_addvar(second.model, 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(n_variables(second.model), 4);
    EXPECT_EQ(BIYOS_freemdl(second.model), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(second.env), BIYOS_OK);
}

void attempt(Delivery &delivery, const Held &source, const Held &target)
{
    BIYOS_model *copy = nullptr;
    delivery.statuses.push_back(BIYOS_copymdl(source.model, target.env, &copy));
    delivery.nulls.push_back(copy == nullptr);
    EXPECT_EQ(BIYOS_freemdl(copy), BIYOS_OK);
}

void copy_inside(const BIYOS_event *, void *data)
{
    Delivery &delivery = *static_cast<Delivery *>(data);
    attempt(delivery, delivery.busy, delivery.busy);
    attempt(delivery, delivery.busy, delivery.idle);
    attempt(delivery, delivery.idle, delivery.busy);
    attempt(delivery, delivery.idle, delivery.idle);
}

void keep(const BIYOS_event *event, void *data)
{
    static_cast<std::vector<std::string> *>(data)->emplace_back(BIYOS_getevttext(event));
}

int add_hook(const Env &env, std::vector<std::string> &texts)
{
    BIYOS_index id = 0;
    const int message = BIYOS_EVTKND_MSG;
    EXPECT_EQ(BIYOS_addenvhook(env.get(), keep, &texts, message, BIYOS_EVTLVL_ERR, &id), BIYOS_OK);
    return id;
}

}

TEST(CApiTest, SameCopy)
{
    for (const bool copy_first : {false, true})
    {
        SCOPED_TRACE(copy_first ? "copy first" : "source first");
        BIYOS_env *env = created();
        const Held source = made_on(env);
        const Held copy = copied(source, env);
        EXPECT_STREQ(last_message(env), "");
        free_both(copy_first ? copy : source, copy_first ? source : copy);
    }
}

TEST(CApiTest, OtherCopy)
{
    for (const bool copy_first : {false, true})
    {
        SCOPED_TRACE(copy_first ? "copy first" : "source first");
        const Held source = made_on(created());
        const Held copy = copied(source, created());
        EXPECT_STREQ(last_message(source.env), "");
        EXPECT_STREQ(last_message(copy.env), "");
        free_both(copy_first ? copy : source, copy_first ? source : copy);
    }
}

TEST(CApiTest, HeldCopy)
{
    const std::string held = "invalid argument: environment: has a model that is not freed";
    const Held source = made_on(created());
    const Held same = copied(source, source.env);
    const Held other = copied(source, created());
    EXPECT_EQ(BIYOS_freemdl(source.model), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(source.env), BIYOS_ERR_BADARG);
    EXPECT_EQ(last_message(source.env), held);
    EXPECT_EQ(BIYOS_freemdl(same.model), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(source.env), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(other.env), BIYOS_ERR_BADARG);
    EXPECT_EQ(last_message(other.env), held);
    EXPECT_EQ(BIYOS_freemdl(other.model), BIYOS_OK);
    EXPECT_EQ(BIYOS_freeenv(other.env), BIYOS_OK);
}

TEST(CApiTest, LockedCopy)
{
    const Env busy;
    const Env idle;
    const Model on_busy(busy);
    const Model on_idle(idle);
    Delivery delivery;
    delivery.busy = Held{.env = busy.get(), .model = on_busy.get()};
    delivery.idle = Held{.env = idle.get(), .model = on_idle.get()};
    std::vector<std::string> texts;
    const int kept = add_hook(idle, texts);
    BIYOS_index id = 0;
    const int message = BIYOS_EVTKND_MSG;
    ASSERT_EQ(BIYOS_addenvhook(busy.get(), copy_inside, &delivery, message, BIYOS_EVTLVL_ERR, &id),
              BIYOS_OK);
    EXPECT_EQ(BIYOS_setenvintopt(busy.get(), BIYOS_OPT_SEED, -1), BIYOS_ERR_BADARG);
    const BIYOS_status refused = BIYOS_ERR_BADARG;
    const std::vector<BIYOS_status> statuses{refused, BIYOS_OK, refused, BIYOS_OK};
    EXPECT_EQ(delivery.statuses, statuses);
    EXPECT_EQ(delivery.nulls, (std::vector<bool>{true, false, true, false}));
    EXPECT_EQ(idle.message(), kLocked);
    EXPECT_EQ(texts, std::vector<std::string>{kLocked});
    EXPECT_EQ(busy.message(), "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(BIYOS_delenvhook(busy.get(), id), BIYOS_OK);
    EXPECT_EQ(BIYOS_delenvhook(idle.get(), kept), BIYOS_OK);
}

TEST(CApiTest, NullCopy)
{
    const Env env;
    const Model model(env);
    BIYOS_model *copy = model.get();
    EXPECT_EQ(BIYOS_copymdl(nullptr, env.get(), &copy), BIYOS_ERR_BADARG);
    EXPECT_EQ(copy, nullptr);
    EXPECT_EQ(env.message(), "");
    copy = model.get();
    EXPECT_EQ(BIYOS_copymdl(model.get(), nullptr, &copy), BIYOS_ERR_BADARG);
    EXPECT_EQ(copy, nullptr);
    EXPECT_EQ(env.message(), "invalid argument: argument 'env': is null");
    EXPECT_EQ(BIYOS_copymdl(model.get(), env.get(), nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env.message(), "invalid argument: argument 'copy': is null");
}

TEST(CApiTest, CopyReports)
{
    const Env from;
    const Env to;
    std::vector<std::string> sent;
    std::vector<std::string> received;
    const int first = add_hook(from, sent);
    const int second = add_hook(to, received);
    const Held source = made_on(from.get());
    const Held copy = copied(source, to.get());
    const BIYOS_index index = 7;
    const BIYOS_real bound = 0.0;
    const std::string unknown = "invalid argument: variable 7: unknown";
    EXPECT_EQ(BIYOS_setvarbndsp(copy.model, 1, &index, &bound, &bound), BIYOS_ERR_BADARG);
    EXPECT_EQ(to.message(), unknown);
    EXPECT_EQ(received, std::vector<std::string>{unknown});
    EXPECT_EQ(from.message(), "");
    EXPECT_EQ(sent, std::vector<std::string>{});
    EXPECT_EQ(BIYOS_freemdl(copy.model), BIYOS_OK);
    EXPECT_EQ(BIYOS_freemdl(source.model), BIYOS_OK);
    EXPECT_EQ(BIYOS_delenvhook(from.get(), first), BIYOS_OK);
    EXPECT_EQ(BIYOS_delenvhook(to.get(), second), BIYOS_OK);
}
