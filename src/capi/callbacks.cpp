#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/index.hpp"
#include "model/callbacks.hpp"
#include "model/compressed.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

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
            .x = start_of(request.point),
            .w = start_of(request.weights),
            .v = start_of(request.direction),
        };
        return function(&view, out.data(), data);
    }
};

void add_callback(BIYOS_model &model, int n, const int *functions, void *data, int *cb)
{
    biyos::Model &core = writable(model);
    const int index = core.add_callback(biyos::capi::listed(n, functions, "funidx"), data);
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
                  const std::int64_t *off,
                  const int *variables,
                  BIYOS_cbjac jac)
{
    biyos::Model &core = writable(model);
    const Caller caller{require(jac, "jac")};
    const std::size_t n = core.callback(cb).covered.size();
    const std::vector<biyos::Offset> starts = biyos::capi::offsets(n, off);
    biyos::check_starts(starts);
    const auto total = static_cast<std::size_t>(starts.back());
    if (total > 0)
    {
        require(variables, "varidx");
    }
    core.set_callback_jacobian(cb, starts, {variables, total}, caller);
}

void set_hessian(BIYOS_model &model,
                 int cb,
                 std::int64_t nnz,
                 const int *first,
                 const int *second,
                 BIYOS_cbhess hess)
{
    biyos::Model &core = writable(model);
    const Caller caller{require(hess, "hess")};
    const std::size_t size = length(nnz, "nnz");
    if (size > 0)
    {
        require(first, "varidx1");
        require(second, "varidx2");
    }
    core.set_callback_hessian(cb, {first, size}, {second, size}, caller);
}

void set_product(BIYOS_model &model, int cb, BIYOS_cbhessvec hessvec)
{
    biyos::Model &core = writable(model);
    core.set_callback_hessian_product(cb, Caller{require(hessvec, "hessvec")});
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

}

BIYOS_status BIYOS_addcb(BIYOS_model *mdl,
                         BIYOS_index n,
                         const BIYOS_index *funidx,
                         void *data,
                         BIYOS_index *cb)
{
    return guarded<add_callback>(mdl, n, funidx, data, cb);
}

BIYOS_status BIYOS_setcbf(BIYOS_model *mdl, BIYOS_index cb, BIYOS_cbf f)
{
    return guarded<set_values>(mdl, cb, f);
}

BIYOS_status BIYOS_setcbjac(BIYOS_model *mdl,
                            BIYOS_index cb,
                            const BIYOS_offset *off,
                            const BIYOS_index *varidx,
                            BIYOS_cbjac jac)
{
    return guarded<set_jacobian>(mdl, cb, off, varidx, jac);
}

BIYOS_status BIYOS_setcbhess(BIYOS_model *mdl,
                             BIYOS_index cb,
                             BIYOS_offset nnz,
                             const BIYOS_index *varidx1,
                             const BIYOS_index *varidx2,
                             BIYOS_cbhess hess)
{
    return guarded<set_hessian>(mdl, cb, nnz, varidx1, varidx2, hess);
}

BIYOS_status BIYOS_setcbhessvec(BIYOS_model *mdl, BIYOS_index cb, BIYOS_cbhessvec hessvec)
{
    return guarded<set_product>(mdl, cb, hessvec);
}

BIYOS_status BIYOS_setcbdata(BIYOS_model *mdl, BIYOS_index cb, void *data)
{
    return guarded<set_data>(mdl, cb, data);
}

BIYOS_status BIYOS_getcbdata(const BIYOS_model *mdl, BIYOS_index cb, void **data)
{
    return guarded<get_data>(mdl, cb, data);
}
