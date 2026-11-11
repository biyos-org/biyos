#pragma once

#include "expr/derivative.hpp"
#include "expr/reverse.hpp"
#include "expr/tape.hpp"
#include "expr/value.hpp"

#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace biyos::test
{

inline std::vector<double> values_of(const Tape &tape, std::span<const double> point)
{
    Workspace workspace(tape);
    std::vector<double> values(static_cast<std::size_t>(tape.roots()));
    forward(tape, point, workspace, values);
    return values;
}

inline std::vector<double> jacobian_of(const Tape &tape, std::span<const double> point)
{
    Workspace workspace(tape);
    Adjoints adjoints(tape);
    std::vector<double> values(static_cast<std::size_t>(tape.roots()));
    std::vector<double> gradients(tape.pattern().variables.size());
    jacobian(tape, point, workspace, adjoints, values, gradients);
    return gradients;
}

inline std::vector<double> gradient_of(const Tape &tape,
                                       std::span<const double> point,
                                       std::span<const double> weights)
{
    Workspace workspace(tape);
    Adjoints adjoints(tape);
    std::vector<double> values(static_cast<std::size_t>(tape.roots()));
    std::vector<double> gradient(point.size(), (std::numeric_limits<double>::quiet_NaN)());
    weighted_gradient(tape, point, weights, workspace, adjoints, values, gradient);
    return gradient;
}

}
