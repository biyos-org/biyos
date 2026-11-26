#pragma once

#include "core/numbers.hpp"

#include <vector>

namespace biyos
{

struct LinearTerm
{
    Index variable;
    Real coefficient;
};

struct QuadraticTerm
{
    Index row;
    Index column;
    Real coefficient;
};

void add_alike(std::vector<LinearTerm> &terms);
void add_alike(std::vector<QuadraticTerm> &terms);

}
