#pragma once

#include "core/numbers.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/sparsity/reach.hpp"
#include "model/model.hpp"

#include <span>
#include <vector>

namespace biyos::evaluation
{

struct Layout
{
    struct Starts
    {
        std::vector<Offset> linear;
        std::vector<Offset> quadratic;
        std::vector<Offset> callbacks;
    };

    struct Slots
    {
        std::vector<Offset> quadratic;
        std::vector<Offset> tape;
        std::vector<Offset> callbacks;
    };

    Pattern pattern;
    Starts starts;
    Slots slots;
};

struct Entries
{
    std::vector<Entry> items;
    std::vector<Offset> starts;
};

Layout layout_of(const Model &model, std::span<const Index> rooted, const Pattern &tape);
std::vector<Real> linear_of(const Model &model, const Layout &layout);
Entries entries_of(const Model &model);

}
