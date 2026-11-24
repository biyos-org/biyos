#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "capi/model/selection.hpp"
#include "capi/model/tolerances.hpp"
#include "core/index.hpp"
#include "model/compressed.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

using biyos::ConeKind;
using biyos::Family;
using biyos::Index;
using biyos::Model;
using biyos::Offset;
using biyos::capi::check_capacity;
using biyos::capi::delete_tolerances;
using biyos::capi::get_tolerances;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::offsets;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::set_tolerances;
using biyos::capi::writable;

namespace
{

struct Given
{
    const int *kinds;
    const std::int64_t *starts;
    const int *functions;
    const std::int64_t *parameter_starts;
    const double *parameters;
};

template <typename T> using Part = std::span<const T> (Model::*)(Index, Index) const;

template <typename T> struct Compressed
{
    std::int64_t *starts;
    T *items;
};

constexpr Part<Index> kMembers = &Model::cone_functions;
constexpr Part<double> kParameters = &Model::cone_parameters;

std::vector<ConeKind> kinds_of(std::size_t n, const int *kinds)
{
    std::vector<ConeKind> found;
    found.reserve(n);
    for (std::size_t k = 0; k < n; ++k)
    {
        found.push_back(static_cast<ConeKind>(kinds[k]));
    }
    return found;
}

template <typename T>
std::span<const T> items_of(const T *items, std::span<const Offset> starts, std::string_view name)
{
    const auto total = static_cast<std::size_t>(starts.back());
    if (total > 0)
    {
        require(items, name);
    }
    return {items, total};
}

void add_cones(BIYOS_model &model, int n, const Given &given, int *start)
{
    Model &core = writable(model);
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(given.kinds, "knd");
        require(given.starts, "off");
    }
    const std::vector<Offset> starts = offsets(size, given.starts);
    const std::vector<Offset> parameter_starts = offsets(size, given.parameter_starts);
    biyos::check_starts(starts);
    biyos::check_starts(parameter_starts, "prmoff");
    core.check_cone_room(size, starts.back(), parameter_starts.back());
    const Index index = core.n_cones();
    core.add_cones(kinds_of(size, given.kinds),
                   starts,
                   items_of(given.functions, starts, "fun"),
                   parameter_starts,
                   items_of(given.parameters, parameter_starts, "prm"));
    if (start != nullptr)
    {
        *start = index;
    }
}

void n_cones(const BIYOS_model &model, int *n)
{
    *require(n, "n") = model.core.n_cones();
}

template <typename T>
std::size_t count_of(const Model &core, std::span<const Index> cones, Part<T> part)
{
    std::size_t count = 0;
    for (const Index cone : cones)
    {
        count += (core.*part)(cone, cone + 1).size();
    }
    return count;
}

template <typename T>
void write_part(const Model &core, std::span<const Index> cones, Part<T> part, Compressed<T> out)
{
    std::int64_t written = 0;
    for (std::size_t j = 0; j < cones.size(); ++j)
    {
        if (out.starts != nullptr)
        {
            out.starts[j] = written;
        }
        const std::span<const T> items = (core.*part)(cones[j], cones[j] + 1);
        if (out.items != nullptr)
        {
            std::ranges::copy(items, out.items + written);
        }
        written += static_cast<std::int64_t>(items.size());
    }
    if (out.starts != nullptr)
    {
        out.starts[cones.size()] = written;
    }
}

void write_kinds(const Model &core, std::span<const Index> cones, int *kinds)
{
    if (kinds == nullptr)
    {
        return;
    }
    for (std::size_t j = 0; j < cones.size(); ++j)
    {
        kinds[j] = static_cast<int>(core.cone_kinds(cones[j], cones[j] + 1).front());
    }
}

template <typename Selection, typename T>
void n_items(const BIYOS_model &model, Selection selection, Part<T> part, std::int64_t *nnz)
{
    const auto chosen = selected(model.core, Family::Cone, selection);
    const std::size_t count = count_of(model.core, chosen, part);
    *require(nnz, "nnz") = static_cast<std::int64_t>(count);
}

template <typename Selection>
void get_members(const BIYOS_model &model,
                 Selection selection,
                 std::int64_t capacity,
                 int *kinds,
                 Compressed<Index> out)
{
    const auto chosen = selected(model.core, Family::Cone, selection);
    check_capacity(capacity, count_of(model.core, chosen, kMembers), "cap", "members");
    write_kinds(model.core, chosen, kinds);
    write_part(model.core, chosen, kMembers, out);
}

template <typename Selection>
void get_parameters(const BIYOS_model &model,
                    Selection selection,
                    std::int64_t capacity,
                    Compressed<double> out)
{
    const auto chosen = selected(model.core, Family::Cone, selection);
    check_capacity(capacity, count_of(model.core, chosen, kParameters), "cap", "parameters");
    write_part(model.core, chosen, kParameters, out);
}

}

BIYOS_status BIYOS_addcone(BIYOS_model *mdl,
                           BIYOS_index n,
                           const BIYOS_int *knd,
                           const BIYOS_offset *off,
                           const BIYOS_index *fun,
                           const BIYOS_offset *prmoff,
                           const BIYOS_real *prm,
                           BIYOS_index *pos)
{
    const Given given{
        .kinds = knd,
        .starts = off,
        .functions = fun,
        .parameter_starts = prmoff,
        .parameters = prm,
    };
    return guarded<add_cones>(mdl, n, given, pos);
}

BIYOS_status BIYOS_setconetolsp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const BIYOS_real *atol,
                                const BIYOS_real *rtol)
{
    return guarded<set_tolerances<List>>(mdl, Family::Cone, List{n, idx}, atol, rtol);
}

BIYOS_status BIYOS_setconetolrg(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const BIYOS_real *atol,
                                const BIYOS_real *rtol)
{
    return guarded<set_tolerances<Range>>(mdl, Family::Cone, Range{beg, end}, atol, rtol);
}

BIYOS_status BIYOS_getconetolsp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                BIYOS_real *atol,
                                BIYOS_real *rtol)
{
    return guarded<get_tolerances<List>>(mdl, Family::Cone, List{n, idx}, atol, rtol);
}

BIYOS_status BIYOS_getconetolrg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                BIYOS_real *atol,
                                BIYOS_real *rtol)
{
    return guarded<get_tolerances<Range>>(mdl, Family::Cone, Range{beg, end}, atol, rtol);
}

BIYOS_status BIYOS_delconetolsp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *idx)
{
    return guarded<delete_tolerances<List>>(mdl, Family::Cone, List{n, idx});
}

BIYOS_status BIYOS_delconetolrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<delete_tolerances<Range>>(mdl, Family::Cone, Range{beg, end});
}

BIYOS_status BIYOS_getncone(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_cones>(mdl, n);
}

BIYOS_status BIYOS_getnconefunsp(const BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 BIYOS_offset *nnz)
{
    return guarded<n_items<List, Index>>(mdl, List{n, idx}, kMembers, nnz);
}

BIYOS_status BIYOS_getnconefunrg(const BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 BIYOS_offset *nnz)
{
    return guarded<n_items<Range, Index>>(mdl, Range{beg, end}, kMembers, nnz);
}

BIYOS_status BIYOS_getconesp(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *idx,
                             BIYOS_offset cap,
                             BIYOS_int *knd,
                             BIYOS_offset *off,
                             BIYOS_index *fun)
{
    const Compressed<Index> out{.starts = off, .items = fun};
    return guarded<get_members<List>>(mdl, List{n, idx}, cap, knd, out);
}

BIYOS_status BIYOS_getconerg(const BIYOS_model *mdl,
                             BIYOS_index beg,
                             BIYOS_index end,
                             BIYOS_offset cap,
                             BIYOS_int *knd,
                             BIYOS_offset *off,
                             BIYOS_index *fun)
{
    const Compressed<Index> out{.starts = off, .items = fun};
    return guarded<get_members<Range>>(mdl, Range{beg, end}, cap, knd, out);
}

BIYOS_status BIYOS_getnconeprmsp(const BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 BIYOS_offset *nnz)
{
    return guarded<n_items<List, double>>(mdl, List{n, idx}, kParameters, nnz);
}

BIYOS_status BIYOS_getnconeprmrg(const BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 BIYOS_offset *nnz)
{
    return guarded<n_items<Range, double>>(mdl, Range{beg, end}, kParameters, nnz);
}

BIYOS_status BIYOS_getconeprmsp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                BIYOS_offset cap,
                                BIYOS_offset *prmoff,
                                BIYOS_real *prm)
{
    const Compressed<double> out{.starts = prmoff, .items = prm};
    return guarded<get_parameters<List>>(mdl, List{n, idx}, cap, out);
}

BIYOS_status BIYOS_getconeprmrg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                BIYOS_offset cap,
                                BIYOS_offset *prmoff,
                                BIYOS_real *prm)
{
    const Compressed<double> out{.starts = prmoff, .items = prm};
    return guarded<get_parameters<Range>>(mdl, Range{beg, end}, cap, out);
}
