#pragma once

#include "model/evaluation/evaluation.hpp"
#include "model/session/session.hpp"

namespace biyos::capi
{

Evaluation &evaluation_of(biyos::Session &session);

class Running
{
  public:
    explicit Running(biyos::Session &session) noexcept;
    ~Running();
    Running(const Running &) = delete;
    Running &operator=(const Running &) = delete;

  private:
    biyos::Session &session_;
};

}
