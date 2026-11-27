#pragma once

#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "expr/evaluation/derivatives/reverse.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/tape/tape.hpp"

#include <span>

namespace biyos
{

void check_jacobian(const Tape &tape,
                    std::span<const Real> point,
                    const Workspace &workspace,
                    const Adjoints &adjoints,
                    std::span<const Real> values,
                    std::span<const Real> gradients);
void check_weighted(const Tape &tape,
                    std::span<const Real> point,
                    std::span<const Real> weights,
                    const Workspace &workspace,
                    const Adjoints &adjoints,
                    std::span<const Real> values,
                    std::span<const Real> gradient);
void differentiate(const Tape &tape,
                   const Block &block,
                   const Workspace &workspace,
                   const Workspace &prepass,
                   Adjoints &adjoints,
                   std::span<Real> gradients) noexcept;
void seed_weights(const Block &block, std::span<const Real> weights, Adjoints &adjoints) noexcept;
void pull(const Block &block, std::span<Real> shared, Adjoints &adjoints) noexcept;
void jacobian(const Tape &tape,
              std::span<const Real> point,
              const Tolerances &tolerances,
              Workspace &workspace,
              Adjoints &adjoints,
              std::span<Real> values,
              std::span<Real> gradients);
void weighted_gradient(const Tape &tape,
                       std::span<const Real> point,
                       std::span<const Real> weights,
                       const Tolerances &tolerances,
                       Workspace &workspace,
                       Adjoints &adjoints,
                       std::span<Real> values,
                       std::span<Real> gradient);

}
