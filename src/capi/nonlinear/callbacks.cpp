#include "biyos/biyos.h"

#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/error.hpp"
#include "core/numbers.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

using biyos::Callback;
using biyos::Index;
using biyos::Model;
using biyos::Real;
using biyos::Request;
using biyos::Session;
using biyos::to_index;
using biyos::capi::check_capacity;
using biyos::capi::guarded;
using biyos::capi::handle_of;
using biyos::capi::length;
using biyos::capi::listed;
using biyos::capi::require;
using biyos::capi::writable;
using biyos::capi::write_array;
using biyos::capi::write_value;

namespace
{

const BIYOS_real *start_of(std::span<const Real> values) noexcept
{
    return values.empty() ? nullptr : values.data();
}

struct Caller
{
    BIYOS_cbf function;

    int operator()(const Request &request, std::span<Real> out, void *data) const
    {
        const BIYOS_cbreq view{
            .size = sizeof(BIYOS_cbreq),
            .mdl = handle_of(request.session),
            .cb = request.callback,
            .thread = request.thread,
            .x = start_of(request.x),
            .w = start_of(request.w),
            .v = start_of(request.v),
        };
        return function(&view, out.data(), data);
    }
};

void check_dense(const BIYOS_index *variables)
{
    if (variables != nullptr)
    {
        throw biyos::Error(biyos::ErrorKind::InvalidArgument,
                           "argument 'var'",
                           "is not null for BIYOS_DENSE");
    }
}

void add_callback(Session &session,
                  BIYOS_index m,
                  const BIYOS_index *functions,
                  BIYOS_index n,
                  const BIYOS_index *variables,
                  void *data,
                  BIYOS_index *cb)
{
    Model &model = writable(session);
    const std::span<const Index> covered = listed(m, functions, "fun", "m");
    if (n == BIYOS_DENSE)
    {
        check_dense(variables);
        write_value(model.add_callback(covered, data), cb);
        return;
    }
    write_value(model.add_callback(covered, listed(n, variables, "var"), data), cb);
}

void set_values(Session &session, BIYOS_index cb, BIYOS_cbf f)
{
    Model &model = writable(session);
    model.set_callback_function(cb, Caller{require(f, "f")});
}

void set_jacobian(Session &session,
                  BIYOS_index cb,
                  BIYOS_offset nnz,
                  const BIYOS_index *functions,
                  const BIYOS_index *variables,
                  BIYOS_cbg g)
{
    Model &model = writable(session);
    const Caller caller{require(g, "g")};
    const std::size_t size = length(nnz, "nnz");
    if (size > 0)
    {
        require(functions, "fun");
        require(variables, "var");
    }
    model.set_callback_jacobian(cb, {functions, size}, {variables, size}, caller);
}

void set_hessian(Session &session,
                 BIYOS_index cb,
                 BIYOS_offset nnz,
                 const BIYOS_index *rows,
                 const BIYOS_index *columns,
                 BIYOS_cbh h)
{
    Model &model = writable(session);
    const Caller caller{require(h, "h")};
    const std::size_t size = length(nnz, "nnz");
    if (size > 0)
    {
        require(rows, "var1");
        require(columns, "var2");
    }
    model.set_callback_hessian(cb, {rows, size}, {columns, size}, caller);
}

void set_product(Session &session, BIYOS_index cb, BIYOS_cbhv hv)
{
    Model &model = writable(session);
    model.set_callback_hessian_product(cb, Caller{require(hv, "hv")});
}

void set_data(Session &session, BIYOS_index cb, void *data)
{
    writable(session).set_callback_data(cb, data);
}

void get_data(const Session &session, BIYOS_index cb, void **data)
{
    void **out = require(data, "data");
    *out = session.model().callback(cb).data;
}

void n_functions(const Session &session, BIYOS_index cb, BIYOS_index *n)
{
    BIYOS_index *out = require(n, "n");
    *out = to_index(session.model().callback(cb).covered.size());
}

void n_variables(const Session &session, BIYOS_index cb, BIYOS_index *n)
{
    BIYOS_index *out = require(n, "n");
    const Callback &held = session.model().callback(cb);
    *out = held.dense ? BIYOS_DENSE : to_index(held.variables.size());
}

void get_variables(const Session &session,
                   BIYOS_index cb,
                   BIYOS_index capacity,
                   BIYOS_index *variables)
{
    const Callback &held = session.model().callback(cb);
    if (held.dense)
    {
        biyos::refuse("callback", cb, "reads every variable");
    }
    check_capacity(capacity, held.variables.size(), "cap", "variables");
    write_array(held.variables, variables);
}

void get_functions(const Session &session,
                   BIYOS_index cb,
                   BIYOS_index capacity,
                   BIYOS_index *functions)
{
    const std::vector<Index> &covered = session.model().callback(cb).covered;
    check_capacity(capacity, covered.size(), "cap", "functions");
    if (functions != nullptr)
    {
        std::ranges::copy(covered, functions);
    }
}

}

BIYOS_status BIYOS_addcb(BIYOS_model *mdl,
                         BIYOS_index m,
                         const BIYOS_index *fun,
                         BIYOS_index n,
                         const BIYOS_index *var,
                         void *data,
                         BIYOS_index *cb)
{
    return guarded<add_callback>(mdl, m, fun, n, var, data, cb);
}

BIYOS_status BIYOS_setcbf(BIYOS_model *mdl, BIYOS_index cb, BIYOS_cbf f)
{
    return guarded<set_values>(mdl, cb, f);
}

BIYOS_status BIYOS_setcbg(BIYOS_model *mdl,
                          BIYOS_index cb,
                          BIYOS_offset nnz,
                          const BIYOS_index *fun,
                          const BIYOS_index *var,
                          BIYOS_cbg g)
{
    return guarded<set_jacobian>(mdl, cb, nnz, fun, var, g);
}

BIYOS_status BIYOS_setcbh(BIYOS_model *mdl,
                          BIYOS_index cb,
                          BIYOS_offset nnz,
                          const BIYOS_index *var1,
                          const BIYOS_index *var2,
                          BIYOS_cbh h)
{
    return guarded<set_hessian>(mdl, cb, nnz, var1, var2, h);
}

BIYOS_status BIYOS_setcbhv(BIYOS_model *mdl, BIYOS_index cb, BIYOS_cbhv hv)
{
    return guarded<set_product>(mdl, cb, hv);
}

BIYOS_status BIYOS_setcbdata(BIYOS_model *mdl, BIYOS_index cb, void *data)
{
    return guarded<set_data>(mdl, cb, data);
}

BIYOS_status BIYOS_getcbdata(const BIYOS_model *mdl, BIYOS_index cb, void **data)
{
    return guarded<get_data>(mdl, cb, data);
}

BIYOS_status BIYOS_getncbfun(const BIYOS_model *mdl, BIYOS_index cb, BIYOS_index *n)
{
    return guarded<n_functions>(mdl, cb, n);
}

BIYOS_status BIYOS_getcbfun(const BIYOS_model *mdl,
                            BIYOS_index cb,
                            BIYOS_index cap,
                            BIYOS_index *fun)
{
    return guarded<get_functions>(mdl, cb, cap, fun);
}

BIYOS_status BIYOS_getncbvar(const BIYOS_model *mdl, BIYOS_index cb, BIYOS_index *n)
{
    return guarded<n_variables>(mdl, cb, n);
}

BIYOS_status BIYOS_getcbvar(const BIYOS_model *mdl,
                            BIYOS_index cb,
                            BIYOS_index cap,
                            BIYOS_index *var)
{
    return guarded<get_variables>(mdl, cb, cap, var);
}
