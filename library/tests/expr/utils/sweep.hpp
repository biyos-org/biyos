#pragma once

#include "core/numbers.hpp"
#include "expr/evaluation/derivatives/derivative.hpp"
#include "expr/evaluation/derivatives/reverse.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/tape/tape.hpp"

#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace biyos::test
{

inline std::vector<Real> values_of(const Tape &tape, std::span<const Real> point)
{
    Workspace workspace(tape);
    std::vector<Real> values(static_cast<std::size_t>(tape.roots()));
    forward(tape, point, workspace, values);
    return values;
}

inline std::vector<Real> jacobian_of(const Tape &tape, std::span<const Real> point)
{
    Workspace workspace(tape);
    Adjoints adjoints(tape);
    std::vector<Real> values(static_cast<std::size_t>(tape.roots()));
    std::vector<Real> gradients(tape.pattern().variables.size());
    jacobian(tape, point, workspace, adjoints, values, gradients);
    return gradients;
}

inline std::vector<Real> gradient_of(const Tape &tape,
                                     std::span<const Real> point,
                                     std::span<const Real> weights)
{
    Workspace workspace(tape);
    Adjoints adjoints(tape);
    std::vector<Real> values(static_cast<std::size_t>(tape.roots()));
    std::vector<Real> gradient(point.size(), (std::numeric_limits<Real>::quiet_NaN)());
    weighted_gradient(tape, point, weights, workspace, adjoints, values, gradient);
    return gradient;
}

}
