#pragma once

#include "core/index.hpp"
#include "expr/hessian/elements.hpp"
#include "expr/sparsity/reach.hpp"
#include "model/model.hpp"

#include <span>
#include <vector>

namespace biyos
{

struct Slots
{
    Pattern pattern;
    std::vector<Offset> linear_starts;
    std::vector<Offset> quadratic_starts;
    std::vector<double> linear;
    std::vector<Offset> quadratic;
    std::vector<Offset> tape;
    std::vector<Offset> callbacks;
    std::vector<Offset> callback_starts;
};

struct Declared
{
    std::vector<Entry> entries;
    std::vector<Offset> callback_starts;
};

Slots slots_of(const Model &model, std::span<const Index> rooted, const Pattern &tape);
Declared declared_of(const Model &model);

}
