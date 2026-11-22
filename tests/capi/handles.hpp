#pragma once

#include "biyos/biyos.h"

#include <gtest/gtest.h>

#include <string>

namespace biyos::test
{

inline BIYOS_error last_error(const BIYOS_env *env)
{
    BIYOS_error error{};
    error.size = sizeof error;
    EXPECT_EQ(BIYOS_lasterror(env, &error), BIYOS_OK);
    return error;
}

inline const char *last_message(const BIYOS_env *env)
{
    return last_error(env).msg;
}

class Env
{
  public:
    Env() { EXPECT_EQ(BIYOS_newenv(&pointer_), BIYOS_OK); }
    ~Env() { BIYOS_freeenv(pointer_); }
    Env(const Env &) = delete;
    Env &operator=(const Env &) = delete;

    BIYOS_env *get() const { return pointer_; }
    std::string message() const { return last_message(pointer_); }

  private:
    BIYOS_env *pointer_ = nullptr;
};

class Model
{
  public:
    explicit Model(const Env &env) { EXPECT_EQ(BIYOS_newmdl(env.get(), &pointer_), BIYOS_OK); }
    ~Model() { BIYOS_freemdl(pointer_); }
    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;

    BIYOS_model *get() const { return pointer_; }

  private:
    BIYOS_model *pointer_ = nullptr;
};

}
