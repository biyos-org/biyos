#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/index.hpp"
#include "model/model.hpp"
#include "model/nodes.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

template <typename T> std::span<const T> input(const T *array, std::size_t n, std::string_view name)
{
    if (n > 0)
    {
        require(array, name);
    }
    return {array, n};
}

void write_node(std::int64_t *node, biyos::Offset made)
{
    if (node != nullptr)
    {
        *node = made;
    }
}

void add_variable_node(BIYOS_model &model, int variable, std::int64_t *node)
{
    biyos::Model &core = writable(model);
    write_node(node, core.add_variable_node(variable));
}

void add_node(BIYOS_model &model,
              int op,
              std::int64_t n,
              const std::int64_t *argument,
              const double *values,
              std::int64_t *node)
{
    biyos::Model &core = writable(model);
    const std::span<const biyos::Offset> children = input(argument, length(n), "arg");
    const auto needed = static_cast<std::size_t>(biyos::n_values(op, n));
    write_node(node, core.add_node(op, children, input(values, needed, "val")));
}

}

BIYOS_status BIYOS_addvarnode(BIYOS_model *mdl, BIYOS_index var, BIYOS_offset *node)
{
    return guarded<add_variable_node>(mdl, var, node);
}

BIYOS_status BIYOS_addnode(BIYOS_model *mdl,
                           BIYOS_int op,
                           BIYOS_offset n,
                           const BIYOS_offset *arg,
                           const BIYOS_real *val,
                           BIYOS_offset *node)
{
    return guarded<add_node>(mdl, op, n, arg, val, node);
}
