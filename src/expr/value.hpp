#pragma once

#include "expr/tape.hpp"

#include <span>
#include <vector>

namespace biyos
{

struct Workspace
{
    explicit Workspace(const Tape &tape);

    std::vector<double> shared;
    std::vector<double> slots;
};

void forward(const Tape &tape,
             std::span<const double> point,
             Workspace &workspace,
             std::span<double> values);

}
