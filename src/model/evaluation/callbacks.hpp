#pragma once

#include "core/check.hpp"
#include "core/numbers.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <string_view>

namespace biyos::evaluation
{

enum class Part
{
    Function,
    Jacobian,
    Hessian,
    Product,
};

bool has(const Callback &callback, Part part) noexcept;
void require(const Callback &callback, Index index, Part part);
void require(const Model &model, Part part);
void evaluate(const Checker &checker,
              const Callback &callback,
              Part part,
              const Request &request,
              std::span<Real> out);
std::size_t widest(const Model &model);

}
