#pragma once

#include "core/env/pool.hpp"
#include "core/numbers.hpp"
#include "expr/evaluation/threads/team.hpp"
#include "expr/tape/tape.hpp"

#include <span>

namespace biyos
{

void forward(const Tape &tape,
             std::span<const Real> point,
             Team &team,
             Pool &pool,
             std::span<Real> values);
void jacobian(const Tape &tape,
              std::span<const Real> point,
              Team &team,
              Pool &pool,
              std::span<Real> values,
              std::span<Real> gradients);
void weighted_gradient(const Tape &tape,
                       std::span<const Real> point,
                       std::span<const Real> weights,
                       Team &team,
                       Pool &pool,
                       std::span<Real> values,
                       std::span<Real> gradient);

}
