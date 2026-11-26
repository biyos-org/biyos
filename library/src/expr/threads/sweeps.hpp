#pragma once

#include "core/numbers.hpp"
#include "core/pool/pool.hpp"
#include "expr/tape/tape.hpp"
#include "expr/threads/team.hpp"

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
