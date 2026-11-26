#include "model/evaluation/callbacks.hpp"

#include "core/check.hpp"
#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/options/options.hpp"
#include "core/text/text.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace biyos::evaluation
{

namespace
{

const Routine &function_of(const Callback &callback, Part part) noexcept
{
    switch (part)
    {
    case Part::Function:
        return callback.function;
    case Part::Jacobian:
        return callback.jacobian;
    case Part::Hessian:
        return callback.hessian;
    case Part::Product:
        return callback.hessian_product;
    }
    return callback.function;
}

std::string_view name_of(Part part) noexcept
{
    switch (part)
    {
    case Part::Function:
        return "function";
    case Part::Jacobian:
        return "Jacobian";
    case Part::Hessian:
        return "Hessian";
    case Part::Product:
        return "Hessian product";
    }
    return "function";
}

bool infinite(Real value) noexcept
{
    return !std::isfinite(value);
}

bool never() noexcept
{
    return false;
}

void check_finite(const Checker &checker, Index index, Part part, std::span<const Real> out)
{
    if (!checker.active(Checking::Cheap))
    {
        return;
    }
    const auto bad = std::ranges::find_if(out, infinite);
    if (bad == out.end())
    {
        return;
    }
    const std::string object = item("callback", index);
    const std::string values = std::string(name_of(part)) + " output " +
                               text_of(bad - out.begin()) + " is " + text_of(*bad);
    const auto described = [&values]() -> std::string_view { return values; };
    checker
        .require(Checking::Cheap, ErrorKind::Callback, "finite output", object, never, described);
}

}

bool has(const Callback &callback, Part part) noexcept
{
    return static_cast<bool>(function_of(callback, part));
}

void require(const Callback &callback, Index index, Part part)
{
    if (!has(callback, part))
    {
        const std::string reason = "has no " + std::string(name_of(part));
        throw Error(ErrorKind::Unsupported, item("callback", index), reason);
    }
}

void require(const Model &model, Part part)
{
    for (Index c = 0; c < model.n_callbacks(); ++c)
    {
        require(model.callback(c), c, part);
    }
}

void evaluate(const Checker &checker,
              const Callback &callback,
              Part part,
              const Request &request,
              std::span<Real> out)
{
    require(callback, request.callback, part);
    std::ranges::fill(out, 0.0);
    const int status = function_of(callback, part)(request, out, callback.data);
    if (status < 0)
    {
        const std::string reason = std::string(name_of(part)) + " returned " + text_of(status);
        throw Error(ErrorKind::Callback, item("callback", request.callback), reason);
    }
    if (status > 0)
    {
        std::ranges::fill(out, std::numeric_limits<Real>::quiet_NaN());
        return;
    }
    check_finite(checker, request.callback, part, out);
}

std::size_t widest(const Model &model)
{
    std::size_t size = 0;
    for (Index c = 0; c < model.n_callbacks(); ++c)
    {
        const Callback &callback = model.callback(c);
        size = (std::max)({size,
                           callback.covered.size(),
                           callback.jacobian_pairs.size(),
                           callback.hessian_pairs.size(),
                           to_size(model.n_variables())});
    }
    return size;
}

}
