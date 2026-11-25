#pragma once

#include "biyos/biyos.h"
#include "core/numbers.hpp"

#include <compare>
#include <functional>
#include <span>
#include <vector>

namespace biyos
{

class Session;

struct Request
{
    const Session *session;
    Index callback;
    Index thread;
    std::span<const Real> x;
    std::span<const Real> w;
    std::span<const Real> v;
};

using Routine = std::function<int(const Request &request, std::span<Real> out, void *data)>;

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
