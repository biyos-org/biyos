#include "biyos/biyos.h"
#include "capi/entry.hpp"
#include "handles.hpp"
#include "model/compare.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace
{

using biyos::test::Env;
using biyos::test::Model;

const std::string kLocked =
    "invalid argument: argument 'env': is read-only while delivering an event";

struct Held
{
    biyos_env *env = nullptr;
    biyos_model *model = nullptr;
};

struct Delivery
{
    Held busy;
    Held idle;
    std::vector<biyos_status> statuses;
    std::vector<bool> nulls;
};

void add_sample(biyos_model *model)
{
    const int kinds[] = {BIYOS_VAR_CONTINUOUS, BIYOS_VAR_BINARY, BIYOS_VAR_INTEGER};
    const double lower[] = {-0.0, 0.0, -1.0};
    const double upper[] = {2.5, 1.0, 3.0};
    const double constants[] = {1.5, 0.1};
    const std::int64_t starts[] = {0, 2, 3};
    const int variables[] = {2, 0, 1};
    const double values[] = {-1.0, 2.0, 0.1};
    const int functions[] = {0, 1, 0};
    ASSERT_EQ(biyos_add_variables(model, 3, kinds, lower, upper, nullptr), BIYOS_OK);
    ASSERT_EQ(biyos_add_functions(model, 2, constants, starts, variables, values, nullptr),
              BIYOS_OK);
    ASSERT_EQ(biyos_add_constraints(model, 3, functions, lower, upper, nullptr), BIYOS_OK);
}

biyos_env *created()
{
    biyos_env *env = nullptr;
    EXPECT_EQ(biyos_env_create(&env), BIYOS_OK);
    return env;
}

Held made_on(biyos_env *env)
{
    biyos_model *model = nullptr;
    EXPECT_EQ(biyos_model_create(env, &model), BIYOS_OK);
    add_sample(model);
    return Held{.env = env, .model = model};
}

Held copied(const Held &source, biyos_env *env)
{
    biyos_model *copy = nullptr;
    EXPECT_EQ(biyos_model_copy(source.model, env, &copy), BIYOS_OK);
    EXPECT_NE(copy, nullptr);
    EXPECT_EQ(biyos::difference(source.model->core, copy->core), "");
    return Held{.env = env, .model = copy};
}

int variable_count(biyos_model *model)
{
    int count = -1;
    EXPECT_EQ(biyos_variable_count(model, &count), BIYOS_OK);
    return count;
}

void free_both(const Held &first, const Held &second)
{
    EXPECT_EQ(biyos_model_free(first.model), BIYOS_OK);
    if (first.env != second.env)
    {
        EXPECT_EQ(biyos_env_free(first.env), BIYOS_OK);
    }
    EXPECT_EQ(biyos_add_variables(second.model, 1, nullptr, nullptr, nullptr, nullptr), BIYOS_OK);
    EXPECT_EQ(variable_count(second.model), 4);
    EXPECT_EQ(biyos_model_free(second.model), BIYOS_OK);
    EXPECT_EQ(biyos_env_free(second.env), BIYOS_OK);
}

void attempt(Delivery &delivery, const Held &source, const Held &target)
{
    biyos_model *copy = nullptr;
    delivery.statuses.push_back(biyos_model_copy(source.model, target.env, &copy));
    delivery.nulls.push_back(copy == nullptr);
    EXPECT_EQ(biyos_model_free(copy), BIYOS_OK);
}

void copy_inside(const biyos_event *, void *user_data)
{
    Delivery &delivery = *static_cast<Delivery *>(user_data);
    attempt(delivery, delivery.busy, delivery.busy);
    attempt(delivery, delivery.busy, delivery.idle);
    attempt(delivery, delivery.idle, delivery.busy);
    attempt(delivery, delivery.idle, delivery.idle);
}

void keep(const biyos_event *event, void *user_data)
{
    static_cast<std::vector<std::string> *>(user_data)->emplace_back(biyos_event_text(event));
}

int subscribe(const Env &env, std::vector<std::string> &texts)
{
    int id = 0;
    const int message = BIYOS_EVENT_MESSAGE;
    EXPECT_EQ(biyos_subscribe(env.get(), keep, &texts, message, BIYOS_LEVEL_ERROR, &id), BIYOS_OK);
    return id;
}

}

TEST(CApiTest, SameCopy)
{
    for (const bool copy_first : {false, true})
    {
        SCOPED_TRACE(copy_first ? "copy first" : "source first");
        biyos_env *env = created();
        const Held source = made_on(env);
        const Held copy = copied(source, env);
        EXPECT_STREQ(biyos_last_error_message(env), "");
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
        EXPECT_STREQ(biyos_last_error_message(source.env), "");
        EXPECT_STREQ(biyos_last_error_message(copy.env), "");
        free_both(copy_first ? copy : source, copy_first ? source : copy);
    }
}

TEST(CApiTest, HeldCopy)
{
    const std::string held = "invalid argument: environment: has a model that is not freed";
    const Held source = made_on(created());
    const Held same = copied(source, source.env);
    const Held other = copied(source, created());
    EXPECT_EQ(biyos_model_free(source.model), BIYOS_OK);
    EXPECT_EQ(biyos_env_free(source.env), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(biyos_last_error_message(source.env), held);
    EXPECT_EQ(biyos_model_free(same.model), BIYOS_OK);
    EXPECT_EQ(biyos_env_free(source.env), BIYOS_OK);
    EXPECT_EQ(biyos_env_free(other.env), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(biyos_last_error_message(other.env), held);
    EXPECT_EQ(biyos_model_free(other.model), BIYOS_OK);
    EXPECT_EQ(biyos_env_free(other.env), BIYOS_OK);
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
    const int kept = subscribe(idle, texts);
    int id = 0;
    const int message = BIYOS_EVENT_MESSAGE;
    ASSERT_EQ(biyos_subscribe(busy.get(), copy_inside, &delivery, message, BIYOS_LEVEL_ERROR, &id),
              BIYOS_OK);
    EXPECT_EQ(biyos_set_int_option(busy.get(), BIYOS_OPT_SEED, -1), BIYOS_INVALID_ARGUMENT);
    const biyos_status refused = BIYOS_INVALID_ARGUMENT;
    const std::vector<biyos_status> statuses{refused, BIYOS_OK, refused, BIYOS_OK};
    EXPECT_EQ(delivery.statuses, statuses);
    EXPECT_EQ(delivery.nulls, (std::vector<bool>{true, false, true, false}));
    EXPECT_EQ(idle.message(), kLocked);
    EXPECT_EQ(texts, std::vector<std::string>{kLocked});
    EXPECT_EQ(busy.message(), "invalid argument: option 'seed': -1 is outside [0, 2147483647]");
    EXPECT_EQ(biyos_unsubscribe(busy.get(), id), BIYOS_OK);
    EXPECT_EQ(biyos_unsubscribe(idle.get(), kept), BIYOS_OK);
}

TEST(CApiTest, NullCopy)
{
    const Env env;
    const Model model(env);
    biyos_model *copy = model.get();
    EXPECT_EQ(biyos_model_copy(nullptr, env.get(), &copy), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(copy, nullptr);
    EXPECT_EQ(env.message(), "");
    copy = model.get();
    EXPECT_EQ(biyos_model_copy(model.get(), nullptr, &copy), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(copy, nullptr);
    EXPECT_EQ(env.message(), "invalid argument: argument 'env': is null");
    EXPECT_EQ(biyos_model_copy(model.get(), env.get(), nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'copy': is null");
}

TEST(CApiTest, CopyReports)
{
    const Env from;
    const Env to;
    std::vector<std::string> sent;
    std::vector<std::string> received;
    const int first = subscribe(from, sent);
    const int second = subscribe(to, received);
    const Held source = made_on(from.get());
    const Held copy = copied(source, to.get());
    const int index = 7;
    const double bound = 0.0;
    const std::string unknown = "invalid argument: variable 7: unknown";
    EXPECT_EQ(biyos_set_variable_bounds(copy.model, 1, &index, &bound, &bound),
              BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(to.message(), unknown);
    EXPECT_EQ(received, std::vector<std::string>{unknown});
    EXPECT_EQ(from.message(), "");
    EXPECT_EQ(sent, std::vector<std::string>{});
    EXPECT_EQ(biyos_model_free(copy.model), BIYOS_OK);
    EXPECT_EQ(biyos_model_free(source.model), BIYOS_OK);
    EXPECT_EQ(biyos_unsubscribe(from.get(), first), BIYOS_OK);
    EXPECT_EQ(biyos_unsubscribe(to.get(), second), BIYOS_OK);
}
