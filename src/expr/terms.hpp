#pragma once

#include "core/index.hpp"

#include <vector>

namespace biyos
{

struct LinearTerm
{
    Index variable;
    double coefficient;
};

struct QuadraticTerm
{
    Index first;
    Index second;
    double coefficient;
};

void add_alike(std::vector<LinearTerm> &terms);
void add_alike(std::vector<QuadraticTerm> &terms);

}
