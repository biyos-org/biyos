#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "core/index.hpp"
#include "model/compressed.hpp"
#include "model/model.hpp"
#include "model/transpose.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::listed;
using biyos::capi::offsets;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

void add_linear_terms(BIYOS_model &model,
                      int n,
                      const int *functions,
                      const std::int64_t *starts,
                      const int *variables,
                      const double *values)
{
    biyos::Model &core = writable(model);
    const std::span<const biyos::Index> owners = listed(n, functions, "funidx");
    const std::vector<biyos::Offset> begins = offsets(owners.size(), starts);
    biyos::check_starts(begins);
    core.check_room(0, begins.back());
    const auto total = static_cast<std::size_t>(begins.back());
    core.add_linear_terms(owners, begins, biyos::capi::packed(total, variables, values));
}

template <typename T> std::span<T> given(T *array, std::size_t size)
{
    return array == nullptr ? std::span<T>() : std::span<T>(array, size);
}

void get_variable_terms(const BIYOS_model &model,
                        int start,
                        int size,
                        std::int64_t capacity,
                        std::int64_t *starts,
                        std::int64_t *needed,
                        int *functions,
                        double *values)
{
    const std::vector<biyos::Offset> found = biyos::variable_starts(model.core, start, size);
    const std::size_t room = length(capacity, "size");
    require(needed, "nnz");
    const std::size_t written = (std::min)(room, static_cast<std::size_t>(found.back()));
    biyos::variable_terms(model.core,
                          start,
                          found,
                          given(functions, written),
                          given(values, written));
    if (starts != nullptr)
    {
        std::ranges::copy(found, starts);
    }
    *needed = found.back();
}

}

BIYOS_status BIYOS_addlinterms(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *funidx,
                               const BIYOS_offset *off,
                               const BIYOS_index *varidx,
                               const BIYOS_real *coef)
{
    return guarded<add_linear_terms>(mdl, n, funidx, off, varidx, coef);
}

BIYOS_status BIYOS_getclintermrg(const BIYOS_model *mdl,
                                 BIYOS_index pos,
                                 BIYOS_index n,
                                 BIYOS_offset cap,
                                 BIYOS_offset *off,
                                 BIYOS_offset *nnz,
                                 BIYOS_index *funidx,
                                 BIYOS_real *coef)
{
    return guarded<get_variable_terms>(mdl, pos, n, cap, off, nnz, funidx, coef);
}
