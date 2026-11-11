#pragma once

#include "expr/tape.hpp"
#include "expr/value.hpp"

#include <cstddef>
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

}
