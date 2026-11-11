#pragma once

#include "biyos/biyos.h"

#include <gtest/gtest.h>

#include <string>

namespace biyos::test
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

}
