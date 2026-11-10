#include "biyos/biyos.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

class Env
{
  public:
    Env() { EXPECT_EQ(biyos_env_create(&ptr_), BIYOS_OK); }
    ~Env() { biyos_env_free(ptr_); }
    Env(const Env &) = delete;
    Env &operator=(const Env &) = delete;

    biyos_env *get() const { return ptr_; }
    std::string message() const { return biyos_last_error_message(ptr_); }

  private:
    biyos_env *ptr_ = nullptr;
};

class Model
{
  public:
    explicit Model(const Env &env) { EXPECT_EQ(biyos_model_create(env.get(), &ptr_), BIYOS_OK); }
    ~Model() { biyos_model_free(ptr_); }
    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;

    biyos_model *get() const { return ptr_; }

  private:
    biyos_model *ptr_ = nullptr;
};

struct Attempt
{
    biyos_env *env = nullptr;
    biyos_model *model = nullptr;
    std::vector<biyos_status> statuses;
    std::vector<std::string> messages;
};

void note(Attempt &attempt, biyos_status status)
{
    attempt.statuses.push_back(status);
    attempt.messages.emplace_back(biyos_last_error_message(attempt.env));
}

void change(const biyos_event *, void *user_data)
{
    Attempt &attempt = *static_cast<Attempt *>(user_data);
    biyos_model *made = nullptr;
    note(attempt, biyos_model_create(attempt.env, &made));
    note(attempt, biyos_model_free(attempt.model));
}

void fail(const Env &env)
{
    EXPECT_EQ(biyos_set_int_option(env.get(), BIYOS_OPT_SEED, -1), BIYOS_INVALID_ARGUMENT);
}

}

TEST(CApiTest, ModelCreate)
{
    const Env env;
    biyos_model *model = nullptr;
    ASSERT_EQ(biyos_model_create(env.get(), &model), BIYOS_OK);
    EXPECT_NE(model, nullptr);
    EXPECT_EQ(env.message(), "");
    EXPECT_EQ(biyos_model_free(model), BIYOS_OK);
}

TEST(CApiTest, NullModel)
{
    const Env env;
    const Model kept(env);
    biyos_model *model = kept.get();
    EXPECT_EQ(biyos_model_create(nullptr, &model), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(model, nullptr);
    EXPECT_EQ(biyos_model_create(env.get(), nullptr), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(env.message(), "invalid argument: argument 'model': is null");
    EXPECT_EQ(biyos_model_free(nullptr), BIYOS_OK);
}

TEST(CApiTest, HeldFree)
{
    biyos_env *env = nullptr;
    biyos_model *first = nullptr;
    biyos_model *second = nullptr;
    ASSERT_EQ(biyos_env_create(&env), BIYOS_OK);
    ASSERT_EQ(biyos_model_create(env, &first), BIYOS_OK);
    ASSERT_EQ(biyos_model_create(env, &second), BIYOS_OK);
    EXPECT_EQ(biyos_env_free(env), BIYOS_INVALID_ARGUMENT);
    EXPECT_STREQ(biyos_last_error_message(env),
                 "invalid argument: environment: has a model that is not freed");
    EXPECT_EQ(biyos_model_free(first), BIYOS_OK);
    EXPECT_EQ(biyos_env_free(env), BIYOS_INVALID_ARGUMENT);
    EXPECT_EQ(biyos_set_int_option(env, BIYOS_OPT_SEED, 5), BIYOS_OK);
    EXPECT_EQ(biyos_model_free(second), BIYOS_OK);
    EXPECT_EQ(biyos_env_free(env), BIYOS_OK);
}

TEST(CApiTest, LockedModel)
{
    const Env env;
    const Model model(env);
    Attempt attempt;
    attempt.env = env.get();
    attempt.model = model.get();
    int id = 0;
    const int message = BIYOS_EVENT_MESSAGE;
    ASSERT_EQ(biyos_subscribe(env.get(), change, &attempt, message, BIYOS_LEVEL_ERROR, &id),
              BIYOS_OK);
    fail(env);
    const std::vector<biyos_status> statuses(2, BIYOS_INVALID_ARGUMENT);
    const std::vector<std::string> messages{
        "invalid argument: environment: is read-only while delivering an event",
        "invalid argument: model: is read-only while delivering an event",
    };
    EXPECT_EQ(attempt.statuses, statuses);
    EXPECT_EQ(attempt.messages, messages);
    EXPECT_EQ(biyos_unsubscribe(env.get(), id), BIYOS_OK);
}
