#pragma once

#include "expr/reverse.hpp"
#include "expr/tape.hpp"
#include "expr/value.hpp"

#include <span>

namespace biyos
{

void jacobian(const Tape &tape,
              std::span<const double> point,
              Workspace &workspace,
              Adjoints &adjoints,
              std::span<double> values,
              std::span<double> gradients);

}
