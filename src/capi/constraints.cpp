#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <limits>
#include <span>
#include <vector>

using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::listed;
using biyos::capi::picked;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

constexpr double kInfinity = std::numeric_limits<double>::infinity();

biyos::Constraint bounded(biyos::Constraint con,
                          std::size_t k,
                          const double *lower,
                          const double *upper)
{
    if (lower != nullptr)
    {
        con.lower = lower[k];
    }
    if (upper != nullptr)
    {
        con.upper = upper[k];
    }
    return con;
}

void add_cons(BIYOS_model &mdl,
              int n,
              const int *funs,
              const double *lower,
              const double *upper,
              int *beg)
{
    biyos::Model &core = writable(mdl);
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(funs, "funidx");
    }
    std::vector<biyos::Constraint> added;
    added.reserve(size);
    for (std::size_t k = 0; k < size; ++k)
    {
        const biyos::Constraint free{
            .function = funs[k],
            .lower = -kInfinity,
            .upper = kInfinity,
        };
        added.push_back(bounded(free, k, lower, upper));
    }
    const biyos::Index start = core.n_cons();
    core.add_cons(added);
    if (beg != nullptr)
    {
        *beg = start;
    }
}

void set_bnds(BIYOS_model &mdl, int n, const int *idx, const double *lower, const double *upper)
{
    biyos::Model &core = writable(mdl);
    const std::span<const biyos::Index> changed = listed(n, idx);
    std::vector<biyos::Constraint> cons;
    cons.reserve(changed.size());
    for (std::size_t k = 0; k < changed.size(); ++k)
    {
        cons.push_back(bounded(core.constraint(changed[k]), k, lower, upper));
    }
    core.set_cons(changed, cons);
}

void n_cons(const BIYOS_model &mdl, int *n)
{
    *require(n, "n") = mdl.core.n_cons();
}

void copy_out(const biyos::Constraint &con, std::size_t k, int *funs, double *lower, double *upper)
{
    if (funs != nullptr)
    {
        funs[k] = con.function;
    }
    if (lower != nullptr)
    {
        lower[k] = con.lower;
    }
    if (upper != nullptr)
    {
        upper[k] = con.upper;
    }
}

void get_cons(const BIYOS_model &mdl, int beg, int n, int *funs, double *lower, double *upper)
{
    const std::span<const biyos::Constraint> found = mdl.core.cons(beg, n);
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        copy_out(found[k], k, funs, lower, upper);
    }
}

void get_bnds(const BIYOS_model &mdl, int n, const int *idx, double *lower, double *upper)
{
    const std::vector<biyos::Constraint> cons =
        picked(mdl.core, &biyos::Model::constraint, listed(n, idx));
    for (std::size_t k = 0; k < cons.size(); ++k)
    {
        copy_out(cons[k], k, nullptr, lower, upper);
    }
}

}

BIYOS_status BIYOS_addcon(BIYOS_model *mdl,
                          BIYOS_index n,
                          const BIYOS_index *funidx,
                          const BIYOS_real *lb,
                          const BIYOS_real *ub,
                          BIYOS_index *beg)
{
    return guarded<add_cons>(mdl, n, funidx, lb, ub, beg);
}

BIYOS_status BIYOS_setconbndsp(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               const BIYOS_real *lb,
                               const BIYOS_real *ub)
{
    return guarded<set_bnds>(mdl, n, idx, lb, ub);
}

BIYOS_status BIYOS_getconbndsp(const BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               BIYOS_real *lb,
                               BIYOS_real *ub)
{
    return guarded<get_bnds>(mdl, n, idx, lb, ub);
}

BIYOS_status BIYOS_getncon(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_cons>(mdl, n);
}

BIYOS_status BIYOS_getconrg(const BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index n,
                            BIYOS_index *funidx,
                            BIYOS_real *lb,
                            BIYOS_real *ub)
{
    return guarded<get_cons>(mdl, beg, n, funidx, lb, ub);
}
