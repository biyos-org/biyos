#pragma once

#include "biyos/biyos.h"
#include "core/index.hpp"

#include <compare>
#include <functional>
#include <span>
#include <vector>

namespace biyos
{

inline constexpr Index kNoCallback = -1;

struct Request
{
    const BIYOS_model *model;
    Index callback;
    Index thread;
    std::span<const double> x;
    std::span<const double> w;
    std::span<const double> v;
};

using UserFunction = std::function<int(const Request &request, std::span<double> out, void *data)>;

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
    UserFunction function;
    std::vector<Offset> jacobian_starts;
    std::vector<Index> jacobian_variables;
    UserFunction jacobian;
    std::vector<IndexPair> hessian_pairs;
    UserFunction hessian;
    UserFunction hessian_product;
};

}
