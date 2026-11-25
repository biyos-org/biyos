#pragma once

#include "core/numbers.hpp"
#include "expr/derivatives/reverse.hpp"
#include "expr/derivatives/value.hpp"
#include "expr/tape/tape.hpp"

#include <span>

namespace biyos
{

void jacobian(const Tape &tape,
              std::span<const Real> point,
              Workspace &workspace,
              Adjoints &adjoints,
              std::span<Real> values,
              std::span<Real> gradients);
void weighted_gradient(const Tape &tape,
                       std::span<const Real> point,
                       std::span<const Real> weights,
                       Workspace &workspace,
                       Adjoints &adjoints,
                       std::span<Real> values,
                       std::span<Real> gradient);

}
