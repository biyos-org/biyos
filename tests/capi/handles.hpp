#pragma once

#include "biyos/biyos.h"

#include <gtest/gtest.h>

#include <string>

namespace biyos::test
{

class Env
{
  public:
    Env() { EXPECT_EQ(BIYOS_newenv(&ptr_), BIYOS_OK); }
    ~Env() { BIYOS_freeenv(ptr_); }
    Env(const Env &) = delete;
    Env &operator=(const Env &) = delete;

    BIYOS_env *get() const { return ptr_; }
    std::string message() const { return BIYOS_lasterror(ptr_); }

  private:
    BIYOS_env *ptr_ = nullptr;
};

class Model
{
  public:
    explicit Model(const Env &env) { EXPECT_EQ(BIYOS_newmdl(env.get(), &ptr_), BIYOS_OK); }
    ~Model() { BIYOS_freemdl(ptr_); }
    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;

    BIYOS_model *get() const { return ptr_; }

  private:
    BIYOS_model *ptr_ = nullptr;
};

}
