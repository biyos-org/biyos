#include "model/model.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/expressions/patterns.hpp"
#include "model/utils/checks.hpp"
#include "model/utils/renumber.hpp"

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
constexpr std::string_view kWord = "cb";

void check_count(std::span<const Index> functions)
{
    if (functions.empty())
    {
        throw Error(ErrorKind::InvalidArgument, "argument 'm'", "0 is not positive");
    }
}

}

Index Model::n_callbacks() const noexcept
{
    return to_index(callbacks_.size());
}

const Callback &Model::callback(Index index) const
{
    check_index(kNoun, index, n_callbacks());
    return callbacks_[to_size(index)];
}

Callback &Model::changed(Index callback)
{
    check_index(kNoun, callback, n_callbacks());
    return callbacks_[to_size(callback)];
}

Index Model::add_callback(std::span<const Index> functions, void *data)
{
    return push_callback(functions, {}, true, data);
}

Index Model::add_callback(std::span<const Index> functions,
                          std::span<const Index> variables,
                          void *data)
{
    return push_callback(functions, variables, false, data);
}

Index Model::push_callback(std::span<const Index> functions,
                           std::span<const Index> variables,
                           bool dense,
                           void *data)
{
    check_n(kNoun, callbacks_, 1);
    check_count(functions);
    check_functions(functions, n_functions(), "fun");
    check_variables(variables, n_variables(), "var");
    const Index index = n_callbacks();
    Callback added{
        .covered = std::vector<Index>(functions.begin(), functions.end()),
        .dense = dense,
        .variables = std::vector<Index>(variables.begin(), variables.end()),
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

void Model::set_callback_function(Index callback, Routine function)
{
    changed(callback).function = std::move(function);
}

void Model::set_callback_jacobian(Index callback,
                                  std::span<const Index> functions,
                                  std::span<const Index> variables,
                                  Routine jacobian)
{
    Callback &held = changed(callback);
    held.jacobian_pairs = jacobian_pairs(callback, held, functions, variables, n_variables());
    held.jacobian = std::move(jacobian);
}

void Model::set_callback_hessian(Index callback,
                                 std::span<const Index> rows,
                                 std::span<const Index> columns,
                                 Routine hessian)
{
    Callback &held = changed(callback);
    held.hessian_pairs = hessian_pairs(callback, held, rows, columns, n_variables());
    held.hessian = std::move(hessian);
}

void Model::set_callback_hessian_product(Index callback, Routine hessian_product)
{
    changed(callback).hessian_product = std::move(hessian_product);
}

void Model::set_callback_data(Index callback, void *data)
{
    changed(callback).data = data;
}

std::vector<Index> Model::delete_callbacks(std::span<const Index> indices)
{
    std::vector<Index> map = renumbering(kNoun, kWord, n_callbacks(), indices);
    keep(callbacks_, map);
    return map;
}

}
