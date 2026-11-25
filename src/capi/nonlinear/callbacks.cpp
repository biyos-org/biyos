#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/index.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using biyos::capi::check_capacity;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

const double *start_of(std::span<const double> values) noexcept
{
    return values.empty() ? nullptr : values.data();
}

struct Caller
{
    BIYOS_cbf function;

    int operator()(const biyos::Request &request, std::span<double> out, void *data) const
    {
        const BIYOS_cbreq view{
            .size = sizeof(BIYOS_cbreq),
            .mdl = request.model,
            .cb = request.callback,
            .thread = request.thread,
            .x = start_of(request.x),
            .w = start_of(request.w),
            .v = start_of(request.v),
        };
        return function(&view, out.data(), data);
    }
};

void add_callback(BIYOS_model &model, int n, const int *functions, void *data, int *cb)
{
    biyos::Model &core = writable(model);
    const int index = core.add_callback(biyos::capi::listed(n, functions, "fun"), data);
    if (cb != nullptr)
    {
        *cb = index;
    }
}

void set_values(BIYOS_model &model, int cb, BIYOS_cbf f)
{
    biyos::Model &core = writable(model);
    core.set_callback_function(cb, Caller{require(f, "f")});
}

void set_jacobian(BIYOS_model &model,
                  int cb,
                  std::int64_t nnz,
                  const int *functions,
                  const int *variables,
                  BIYOS_cbg g)
{
    biyos::Model &core = writable(model);
    const Caller caller{require(g, "g")};
    const std::size_t size = length(nnz, "nnz");
    if (size > 0)
    {
        require(functions, "fun");
        require(variables, "var");
    }
    core.set_callback_jacobian(cb, {functions, size}, {variables, size}, caller);
}

void set_hessian(BIYOS_model &model,
                 int cb,
                 std::int64_t nnz,
                 const int *rows,
                 const int *columns,
                 BIYOS_cbh h)
{
    biyos::Model &core = writable(model);
    const Caller caller{require(h, "h")};
    const std::size_t size = length(nnz, "nnz");
    if (size > 0)
    {
        require(rows, "var1");
        require(columns, "var2");
    }
    core.set_callback_hessian(cb, {rows, size}, {columns, size}, caller);
}

void set_product(BIYOS_model &model, int cb, BIYOS_cbhv hv)
{
    biyos::Model &core = writable(model);
    core.set_callback_hessian_product(cb, Caller{require(hv, "hv")});
}

void set_data(BIYOS_model &model, int cb, void *data)
{
    writable(model).set_callback_data(cb, data);
}

void get_data(const BIYOS_model &model, int cb, void **data)
{
    void **out = require(data, "data");
    *out = model.core.callback(cb).data;
}

void n_functions(const BIYOS_model &model, int cb, int *n)
{
    int *out = require(n, "n");
    *out = static_cast<int>(model.core.callback(cb).covered.size());
}

void get_functions(const BIYOS_model &model, int cb, int capacity, int *functions)
{
    const std::vector<biyos::Index> &covered = model.core.callback(cb).covered;
    check_capacity(capacity, covered.size(), "cap", "functions");
    if (functions != nullptr)
    {
        std::ranges::copy(covered, functions);
    }
}

}

BIYOS_status BIYOS_addcb(BIYOS_model *mdl,
                         BIYOS_index n,
                         const BIYOS_index *fun,
                         void *data,
                         BIYOS_index *cb)
{
    return guarded<add_callback>(mdl, n, fun, data, cb);
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
