#pragma once

#include "biyos/biyos.h"
#include "model/evaluation/evaluation.hpp"

namespace biyos::capi
{

Evaluation &evaluation_of(BIYOS_model &session);

class Running
{
  public:
    explicit Running(BIYOS_model &session) noexcept;
    ~Running();
    Running(const Running &) = delete;
    Running &operator=(const Running &) = delete;

  private:
    BIYOS_model &session_;
};

}
