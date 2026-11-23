#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "capi/selection.hpp"
#include "core/index.hpp"
#include "model/compressed.hpp"
#include "model/model.hpp"
#include "model/nodes.hpp"
#include "model/postfix.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

using biyos::Family;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::offsets;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
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

std::vector<biyos::Offset> starts_of(std::size_t n, const std::int64_t *starts)
{
    if (n > 0)
    {
        require(starts, "off");
    }
    std::vector<biyos::Offset> begins = offsets(n, starts);
    biyos::check_starts(begins);
    return begins;
}

void write_nodes(std::span<const biyos::Offset> tops, std::int64_t *node)
{
    if (node != nullptr)
    {
        std::ranges::copy(tops, node);
    }
}

void add_expressions(BIYOS_model &model,
                     int n,
                     const std::int64_t *starts,
                     const int *code,
                     const std::int64_t *argument,
                     const double *values,
                     std::int64_t *node)
{
    biyos::Model &core = writable(model);
    const std::vector<biyos::Offset> begins = starts_of(length(n), starts);
    const auto total = static_cast<std::size_t>(begins.back());
    const std::span<const int> codes = input(code, total, "code");
    const std::span<const biyos::Offset> arguments = input(argument, total, "arg");
    const auto needed = static_cast<std::size_t>(biyos::n_values(codes, arguments));
    const biyos::Postfix postfix{
        .starts = begins,
        .codes = codes,
        .arguments = arguments,
        .values = input(values, needed, "val"),
    };
    write_nodes(core.add_expressions(postfix), node);
}

template <typename Selection>
void attach_expressions(BIYOS_model &model, Selection selection, const std::int64_t *node)
{
    biyos::Model &core = writable(model);
    const auto owners = selected(core, Family::Function, selection);
    core.attach_expressions(owners, input(node, owners.size(), "node"));
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

BIYOS_status BIYOS_addexpr(BIYOS_model *mdl,
                           BIYOS_index n,
                           const BIYOS_offset *off,
                           const BIYOS_int *code,
                           const BIYOS_offset *arg,
                           const BIYOS_real *val,
                           BIYOS_offset *node)
{
    return guarded<add_expressions>(mdl, n, off, code, arg, val, node);
}

BIYOS_status BIYOS_attachexprsp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const BIYOS_offset *node)
{
    return guarded<attach_expressions<List>>(mdl, List{n, idx}, node);
}

BIYOS_status BIYOS_attachexprrg(BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                const BIYOS_offset *node)
{
    return guarded<attach_expressions<Range>>(mdl, Range{pos, n}, node);
}
