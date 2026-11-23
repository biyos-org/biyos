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
using biyos::capi::check_capacity;
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

template <typename Selection> void clear_functions(BIYOS_model &model, Selection selection)
{
    biyos::Model &core = writable(model);
    core.clear_functions(selected(core, Family::Function, selection));
}

template <typename Selection> biyos::Encoding encoded(const biyos::Model &core, Selection selection)
{
    const auto chosen = selected(core, Family::Function, selection);
    std::vector<biyos::Offset> roots;
    roots.reserve(chosen.size());
    for (const biyos::Index function : chosen)
    {
        roots.push_back(core.roots(function, 1).front());
    }
    return biyos::encode(core.graph(), roots);
}

template <typename Selection>
void n_expressions(const BIYOS_model &model,
                   Selection selection,
                   std::int64_t *nnz,
                   std::int64_t *nval)
{
    const biyos::Encoding encoding = encoded(model.core, selection);
    require(nnz, "nnz");
    require(nval, "nval");
    *nnz = static_cast<std::int64_t>(encoding.codes.size());
    *nval = static_cast<std::int64_t>(encoding.values.size());
}

struct Output
{
    std::int64_t *starts;
    int *codes;
    std::int64_t *arguments;
    double *values;
};

template <typename T> void copy_to(const std::vector<T> &from, T *to)
{
    if (to != nullptr)
    {
        std::ranges::copy(from, to);
    }
}

template <typename Selection>
void get_expressions(const BIYOS_model &model,
                     Selection selection,
                     std::int64_t cap,
                     std::int64_t valcap,
                     const Output &out)
{
    const biyos::Encoding encoding = encoded(model.core, selection);
    check_capacity(cap, encoding.codes.size(), "cap", "entries");
    check_capacity(valcap, encoding.values.size(), "valcap", "values");
    copy_to(encoding.starts, out.starts);
    copy_to(encoding.codes, out.codes);
    copy_to(encoding.arguments, out.arguments);
    copy_to(encoding.values, out.values);
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

BIYOS_status BIYOS_clrfunsp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<clear_functions<List>>(mdl, List{n, idx});
}

BIYOS_status BIYOS_clrfunrg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n)
{
    return guarded<clear_functions<Range>>(mdl, Range{pos, n});
}

BIYOS_status BIYOS_getnexprsp(const BIYOS_model *mdl,
                              BIYOS_index n,
                              const BIYOS_index *idx,
                              BIYOS_offset *nnz,
                              BIYOS_offset *nval)
{
    return guarded<n_expressions<List>>(mdl, List{n, idx}, nnz, nval);
}

BIYOS_status BIYOS_getnexprrg(const BIYOS_model *mdl,
                              BIYOS_index pos,
                              BIYOS_index n,
                              BIYOS_offset *nnz,
                              BIYOS_offset *nval)
{
    return guarded<n_expressions<Range>>(mdl, Range{pos, n}, nnz, nval);
}

BIYOS_status BIYOS_getexprsp(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *idx,
                             BIYOS_offset cap,
                             BIYOS_offset valcap,
                             BIYOS_offset *off,
                             BIYOS_int *code,
                             BIYOS_offset *arg,
                             BIYOS_real *val)
{
    const Output out{
        .starts = off,
        .codes = code,
        .arguments = arg,
        .values = val,
    };
    return guarded<get_expressions<List>>(mdl, List{n, idx}, cap, valcap, out);
}

BIYOS_status BIYOS_getexprrg(const BIYOS_model *mdl,
                             BIYOS_index pos,
                             BIYOS_index n,
                             BIYOS_offset cap,
                             BIYOS_offset valcap,
                             BIYOS_offset *off,
                             BIYOS_int *code,
                             BIYOS_offset *arg,
                             BIYOS_real *val)
{
    const Output out{
        .starts = off,
        .codes = code,
        .arguments = arg,
        .values = val,
    };
    return guarded<get_expressions<Range>>(mdl, Range{pos, n}, cap, valcap, out);
}
