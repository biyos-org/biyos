#pragma once

#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "expr/evaluation/derivatives/reverse.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/tape/tape.hpp"

#include <cstddef>
#include <vector>

namespace biyos
{

struct Velocity
{
    Velocity(const Tape &tape, std::size_t lanes);

    std::vector<Real> slots;
    std::vector<Real> shared;
    std::vector<Real> partials;
    std::vector<Real> running;
    std::vector<Real> prefix;
    std::vector<Real> result;
};

struct Pass
{
    Pass(const Tape &tape, std::size_t count);

    std::size_t lanes;
    Workspace workspace;
    Velocity velocity;
    Adjoints first;
    Adjoints second;
    std::vector<Real> seeds;
};

void sweep(const Tape &tape,
           Offset element,
           std::span<const Real> point,
           const Tolerances &tolerances,
           Pass &pass) noexcept;

}
