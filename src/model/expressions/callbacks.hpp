#pragma once

#include "biyos/biyos.h"
#include "core/numbers.hpp"

#include <compare>
#include <functional>
#include <span>
#include <vector>

namespace biyos
{

struct Request
{
    const BIYOS_model *model;
    Index callback;
    Index thread;
    std::span<const double> x;
    std::span<const double> w;
    std::span<const double> v;
};

using Routine = std::function<int(const Request &request, std::span<double> out, void *data)>;

struct IndexPair
{
    Index row;
    Index column;

    auto operator<=>(const IndexPair &) const = default;
};

struct Callback
{
    std::vector<Index> covered;
    void *data;
    Routine function;
    std::vector<IndexPair> jacobian_pairs;
    Routine jacobian;
    std::vector<IndexPair> hessian_pairs;
    Routine hessian;
    Routine hessian_product;
};

}
