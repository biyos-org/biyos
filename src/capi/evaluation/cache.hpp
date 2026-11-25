#pragma once

#include "biyos/biyos.h"
#include "model/evaluation/evaluation.hpp"

namespace biyos::capi
{

Evaluation &evaluation_of(BIYOS_model &model);

class Running
{
  public:
    explicit Running(BIYOS_model &model) noexcept;
    ~Running();
    Running(const Running &) = delete;
    Running &operator=(const Running &) = delete;

  private:
    BIYOS_model &model_;
};

}
