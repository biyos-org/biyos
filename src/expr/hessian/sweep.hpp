#pragma once

#include "core/index.hpp"
#include "expr/derivatives/reverse.hpp"
#include "expr/derivatives/value.hpp"
#include "expr/tape/tape.hpp"

#include <cstddef>
#include <vector>

namespace biyos
{

struct Velocity
{
    Velocity(const Tape &tape, std::size_t lanes);

    std::vector<double> slots;
    std::vector<double> shared;
    std::vector<double> partials;
    std::vector<double> spare;
    std::vector<double> result;
};

struct Pass
{
    Pass(const Tape &tape, std::size_t count);

    std::size_t lanes;
    Workspace workspace;
    Velocity velocity;
    Adjoints first;
    Adjoints second;
    std::vector<double> points;
    std::vector<double> seeds;
};

void sweep(const Tape &tape, Offset element, Pass &pass) noexcept;

}
