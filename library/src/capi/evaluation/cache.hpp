#pragma once

#include "model/evaluation/evaluation.hpp"
#include "model/session/session.hpp"

namespace biyos::capi
{

Evaluation &evaluation_of(Session &session);

class Running
{
  public:
    explicit Running(Session &session) noexcept;
    ~Running();
    Running(const Running &) = delete;
    Running &operator=(const Running &) = delete;

  private:
    Session &session_;
};

}
