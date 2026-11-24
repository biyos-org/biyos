#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/expressions/patterns.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "callback";

std::size_t at(Index index) noexcept
{
    return static_cast<std::size_t>(index);
}

void check_count(std::span<const Index> functions)
{
    if (functions.empty())
    {
        throw Error(ErrorKind::InvalidArgument, "argument 'n'", "0 is not positive");
    }
}

}

Index Model::n_callbacks() const noexcept
{
    return static_cast<Index>(callbacks_.size());
}

const Callback &Model::callback(Index index) const
{
    check_index(kNoun, index, n_callbacks());
    return callbacks_[at(index)];
}

Callback &Model::changed(Index callback)
{
    check_index(kNoun, callback, n_callbacks());
    return callbacks_[at(callback)];
}

Index Model::add_callback(std::span<const Index> functions, void *data)
{
    check_n(kNoun, callbacks_, 1);
    check_count(functions);
    check_functions(functions, n_functions(), "fun");
    const Index index = n_callbacks();
    Callback added{
        .covered = std::vector<Index>(functions.begin(), functions.end()),
        .data = data,
        .function = {},
        .jacobian_pairs = {},
        .jacobian = {},
        .hessian_pairs = {},
        .hessian = {},
        .hessian_product = {},
    };
    callbacks_.push_back(std::move(added));
    return index;
}

void Model::set_callback_function(Index callback, UserFunction function)
{
    changed(callback).function = std::move(function);
}

void Model::set_callback_jacobian(Index callback,
                                  std::span<const Index> functions,
                                  std::span<const Index> variables,
                                  UserFunction jacobian)
{
    Callback &held = changed(callback);
    held.jacobian_pairs =
        jacobian_pairs(callback, held.covered, functions, variables, n_variables());
    held.jacobian = std::move(jacobian);
}

void Model::set_callback_hessian(Index callback,
                                 std::span<const Index> first,
                                 std::span<const Index> second,
                                 UserFunction hessian)
{
    Callback &held = changed(callback);
    held.hessian_pairs = hessian_pairs(callback, first, second, n_variables());
    held.hessian = std::move(hessian);
}

void Model::set_callback_hessian_product(Index callback, UserFunction hessian_product)
{
    changed(callback).hessian_product = std::move(hessian_product);
}

void Model::set_callback_data(Index callback, void *data)
{
    changed(callback).data = data;
}

}
