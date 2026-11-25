#pragma once

#include "core/env.hpp"
#include "core/index.hpp"
#include "expr/hessian/elements.hpp"
#include "expr/hessian/hessian.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/tape.hpp"
#include "model/evaluation/maps.hpp"
#include "model/model.hpp"

#include <vector>

namespace biyos
{

struct Compiled
{
    std::vector<Index> rooted;
    std::vector<Offset> roots;
    Tape tape;
    Slots slots;
    Declared declared;
    Hessian hessian;
};

class Evaluation
{
  public:
    Evaluation(Env &env, const Model &model);

    const Pattern &jacobian_pattern() const noexcept;
    const Lower &hessian_pattern() const noexcept;

  private:
    Compiled compiled_;
};

}
