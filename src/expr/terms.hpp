#pragma once

#include "core/index.hpp"

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

}
