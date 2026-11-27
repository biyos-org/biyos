#pragma once

#include "core/numbers.hpp"
#include "expr/evaluation/hessian/pattern.hpp"

#include <cstdint>
#include <vector>

namespace biyos
{

struct Elimination
{
    Offset element;
    std::uint32_t variables;
    std::uint32_t root;
    std::vector<Node> nodes;
    std::vector<std::uint32_t> operands;
    std::vector<std::uint32_t> bends;
    std::vector<Offset> lists;
    std::vector<Offset> starts;
    std::vector<std::uint32_t> columns;
    std::vector<Offset> parts;
};

struct PushSpace
{
    std::vector<std::uint32_t> columns;
    std::vector<Real> values;
    std::vector<Offset> sizes;
    std::vector<Offset> marks;
    std::vector<Real> adjoints;
    std::vector<Real> slopes;
    std::vector<Real> bends;
    std::vector<Real> suffix;
};

Elimination elimination_of(Offset element, Shape shape);

}
