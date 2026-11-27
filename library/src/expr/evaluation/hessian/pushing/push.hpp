#pragma once

#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/evaluation/hessian/pushing/lists.hpp"
#include "expr/tape/tape.hpp"

#include <span>

namespace biyos
{

PushSpace space_of(const Tape &tape, std::span<const Elimination> eliminations);
void push(const Tape &tape,
          const Elimination &elimination,
          std::span<const Real> point,
          const Tolerances &tolerances,
          Workspace &workspace,
          PushSpace &space,
          std::span<Real> parts) noexcept;

}
