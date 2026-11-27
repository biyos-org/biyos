#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/expressions/nodes.hpp"
#include "model/expressions/postfix.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <vector>

using biyos::encode;
using biyos::Encoding;
using biyos::Family;
using biyos::Index;
using biyos::Model;
using biyos::n_values;
using biyos::Offset;
using biyos::Postfix;
using biyos::Session;
using biyos::to_offset;
using biyos::to_size;
using biyos::capi::check_capacity;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::span_of;
using biyos::capi::starts_of;
using biyos::capi::writable;
using biyos::capi::write_array;
using biyos::capi::write_counts;
using biyos::capi::write_value;

namespace
{

void add_variable_node(Session &session, BIYOS_index variable, BIYOS_offset *node)
{
    Model &model = writable(session);
    write_value(model.add_variable_node(variable), node);
}

void add_node(Session &session,
              BIYOS_int op,
              BIYOS_offset n,
              const BIYOS_offset *argument,
              const BIYOS_real *values,
              BIYOS_offset *node)
{
    Model &model = writable(session);
    const std::span<const Offset> children = span_of(argument, length(n), "arg");
    const auto needed = to_size(n_values(op, n));
    write_value(model.add_node(op, children, span_of(values, needed, "val")), node);
}

void add_expressions(Session &session,
                     BIYOS_index n,
                     const BIYOS_offset *counts,
                     const BIYOS_int *code,
                     const BIYOS_offset *argument,
                     const BIYOS_real *values,
                     BIYOS_offset *node)
{
    Model &model = writable(session);
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(counts, "cnt");
    }
    const std::vector<Offset> begins = starts_of(size, counts);
    const auto total = to_size(begins.back());
    const std::span<const BIYOS_int> codes = span_of(code, total, "code");
    const std::span<const Offset> arguments = span_of(argument, total, "arg");
    const auto needed = to_size(n_values(codes, arguments));
    const Postfix postfix{
        .starts = begins,
        .codes = codes,
        .arguments = arguments,
        .values = span_of(values, needed, "val"),
    };
    write_array(model.add_expressions(postfix), node);
}

template <typename Selection>
void attach_expressions(Session &session, Selection selection, const BIYOS_offset *node)
{
    Model &model = writable(session);
    const auto owners = selected(Family::Function, model.n_of(Family::Function), selection);
    model.attach_expressions(owners, span_of(node, owners.size(), "node"));
}

Encoding encoded(const Model &model, std::span<const Index> functions)
{
    std::vector<Offset> roots;
    roots.reserve(functions.size());
    for (const Index function : functions)
    {
        roots.push_back(model.roots(function, function + 1).front());
    }
    return encode(model.graph(), roots);
}

template <typename Selection>
void n_expressions(const Session &session,
                   Selection selection,
                   BIYOS_offset *nnz,
                   BIYOS_offset *nval)
{
    const Model &model = session.model();
    const auto functions = selected(Family::Function, model.n_of(Family::Function), selection);
    const Encoding encoding = encoded(model, functions);
    require(nnz, "nnz");
    require(nval, "nval");
    *nnz = to_offset(encoding.codes.size());
    *nval = to_offset(encoding.values.size());
}

struct Output
{
    BIYOS_offset *counts;
    BIYOS_int *codes;
    BIYOS_offset *arguments;
    BIYOS_real *values;
};

template <typename Selection>
void get_expressions(const Session &session,
                     Selection selection,
                     BIYOS_offset cap,
                     BIYOS_offset valcap,
                     const Output &out)
{
    const Model &model = session.model();
    const auto functions = selected(Family::Function, model.n_of(Family::Function), selection);
    const Encoding encoding = encoded(model, functions);
    check_capacity(cap, encoding.codes.size(), "cap", "entries");
    check_capacity(valcap, encoding.values.size(), "valcap", "values");
    write_counts(encoding.starts, out.counts);
    write_array(encoding.codes, out.codes);
    write_array(encoding.arguments, out.arguments);
    write_array(encoding.values, out.values);
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
                           const BIYOS_offset *cnt,
                           const BIYOS_int *code,
                           const BIYOS_offset *arg,
                           const BIYOS_real *val,
                           BIYOS_offset *node)
{
    return guarded<add_expressions>(mdl, n, cnt, code, arg, val, node);
}

BIYOS_status BIYOS_attachexprsp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *fun,
                                const BIYOS_offset *node)
{
    return guarded<attach_expressions<List>>(mdl, List{n, fun}, node);
}

BIYOS_status BIYOS_attachexprrg(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const BIYOS_offset *node)
{
    return guarded<attach_expressions<Range>>(mdl, Range{beg, end}, node);
}

BIYOS_status BIYOS_getnexprsp(const BIYOS_model *mdl,
                              BIYOS_index n,
                              const BIYOS_index *fun,
                              BIYOS_offset *nnz,
                              BIYOS_offset *nval)
{
    return guarded<n_expressions<List>>(mdl, List{n, fun}, nnz, nval);
}

BIYOS_status BIYOS_getnexprrg(const BIYOS_model *mdl,
                              BIYOS_index beg,
                              BIYOS_index end,
                              BIYOS_offset *nnz,
                              BIYOS_offset *nval)
{
    return guarded<n_expressions<Range>>(mdl, Range{beg, end}, nnz, nval);
}

BIYOS_status BIYOS_getexprsp(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *fun,
                             BIYOS_offset cap,
                             BIYOS_offset valcap,
                             BIYOS_offset *cnt,
                             BIYOS_int *code,
                             BIYOS_offset *arg,
                             BIYOS_real *val)
{
    const Output out{
        .counts = cnt,
        .codes = code,
        .arguments = arg,
        .values = val,
    };
    return guarded<get_expressions<List>>(mdl, List{n, fun}, cap, valcap, out);
}

BIYOS_status BIYOS_getexprrg(const BIYOS_model *mdl,
                             BIYOS_index beg,
                             BIYOS_index end,
                             BIYOS_offset cap,
                             BIYOS_offset valcap,
                             BIYOS_offset *cnt,
                             BIYOS_int *code,
                             BIYOS_offset *arg,
                             BIYOS_real *val)
{
    const Output out{
        .counts = cnt,
        .codes = code,
        .arguments = arg,
        .values = val,
    };
    return guarded<get_expressions<Range>>(mdl, Range{beg, end}, cap, valcap, out);
}
