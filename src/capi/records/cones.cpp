#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/model/tolerances.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

using biyos::ConeKind;
using biyos::Family;
using biyos::Index;
using biyos::Model;
using biyos::Offset;
using biyos::Real;
using biyos::to_offset;
using biyos::to_size;
using biyos::capi::check_capacity;
using biyos::capi::delete_tolerances;
using biyos::capi::get_tolerances;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::set_tolerances;
using biyos::capi::starts_of;
using biyos::capi::writable;

namespace
{

struct Given
{
    const BIYOS_int *kinds;
    const BIYOS_offset *counts;
    const BIYOS_index *functions;
    const BIYOS_offset *parameter_counts;
    const BIYOS_real *parameters;
};

template <typename T> using Part = std::span<const T> (Model::*)(Index, Index) const;

template <typename T> struct Compressed
{
    BIYOS_offset *counts;
    T *items;
};

constexpr Part<Index> kMembers = &Model::cone_functions;
constexpr Part<Real> kParameters = &Model::cone_parameters;

std::vector<ConeKind> kinds_of(std::size_t n, const BIYOS_int *kinds)
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
    const auto total = to_size(starts.back());
    if (total > 0)
    {
        require(items, name);
    }
    return {items, total};
}

void add_cones(BIYOS_model &session, BIYOS_index n, const Given &given, BIYOS_index *start)
{
    Model &model = writable(session);
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(given.kinds, "knd");
        require(given.counts, "cnt");
    }
    const std::vector<Offset> starts = starts_of(size, given.counts);
    const std::vector<Offset> parameter_starts = starts_of(size, given.parameter_counts, "prmcnt");
    model.check_cone_room(size, starts.back(), parameter_starts.back());
    const Index index = model.n_cones();
    model.add_cones(kinds_of(size, given.kinds),
                    starts,
                    items_of(given.functions, starts, "fun"),
                    parameter_starts,
                    items_of(given.parameters, parameter_starts, "prm"));
    if (start != nullptr)
    {
        *start = index;
    }
}

void n_cones(const BIYOS_model &session, BIYOS_index *n)
{
    *require(n, "n") = session.model().n_cones();
}

template <typename T>
std::size_t count_of(const Model &model, std::span<const Index> cones, Part<T> part)
{
    std::size_t count = 0;
    for (const Index cone : cones)
    {
        count += (model.*part)(cone, cone + 1).size();
    }
    return count;
}

template <typename T>
void write_part(const Model &model, std::span<const Index> cones, Part<T> part, Compressed<T> out)
{
    BIYOS_offset written = 0;
    for (std::size_t j = 0; j < cones.size(); ++j)
    {
        const std::span<const T> items = (model.*part)(cones[j], cones[j] + 1);
        if (out.counts != nullptr)
        {
            out.counts[j] = to_offset(items.size());
        }
        if (out.items != nullptr)
        {
            std::ranges::copy(items, out.items + written);
        }
        written += to_offset(items.size());
    }
}

void write_kinds(const Model &model, std::span<const Index> cones, BIYOS_int *kinds)
{
    if (kinds == nullptr)
    {
        return;
    }
    for (std::size_t j = 0; j < cones.size(); ++j)
    {
        kinds[j] = static_cast<BIYOS_int>(model.cone_kinds(cones[j], cones[j] + 1).front());
    }
}

template <typename Selection, typename T>
void n_items(const BIYOS_model &session, Selection selection, Part<T> part, BIYOS_offset *nnz)
{
    const auto chosen = selected(session.model(), Family::Cone, selection);
    const std::size_t count = count_of(session.model(), chosen, part);
    *require(nnz, "nnz") = to_offset(count);
}

template <typename Selection>
void get_members(const BIYOS_model &session,
                 Selection selection,
                 BIYOS_offset capacity,
                 BIYOS_int *kinds,
                 Compressed<BIYOS_index> out)
{
    const auto chosen = selected(session.model(), Family::Cone, selection);
    check_capacity(capacity, count_of(session.model(), chosen, kMembers), "cap", "members");
    write_kinds(session.model(), chosen, kinds);
    write_part(session.model(), chosen, kMembers, out);
}

template <typename Selection>
void get_parameters(const BIYOS_model &session,
                    Selection selection,
                    BIYOS_offset capacity,
                    Compressed<BIYOS_real> out)
{
    const auto chosen = selected(session.model(), Family::Cone, selection);
    check_capacity(capacity, count_of(session.model(), chosen, kParameters), "cap", "parameters");
    write_part(session.model(), chosen, kParameters, out);
}

}

BIYOS_status BIYOS_addcone(BIYOS_model *mdl,
                           BIYOS_index n,
                           const BIYOS_int *knd,
                           const BIYOS_offset *cnt,
                           const BIYOS_index *fun,
                           const BIYOS_offset *prmcnt,
                           const BIYOS_real *prm,
                           BIYOS_index *pos)
{
    const Given given{
        .kinds = knd,
        .counts = cnt,
        .functions = fun,
        .parameter_counts = prmcnt,
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
                             BIYOS_offset *cnt,
                             BIYOS_index *fun)
{
    const Compressed<BIYOS_index> out{.counts = cnt, .items = fun};
    return guarded<get_members<List>>(mdl, List{n, idx}, cap, knd, out);
}

BIYOS_status BIYOS_getconerg(const BIYOS_model *mdl,
                             BIYOS_index beg,
                             BIYOS_index end,
                             BIYOS_offset cap,
                             BIYOS_int *knd,
                             BIYOS_offset *cnt,
                             BIYOS_index *fun)
{
    const Compressed<BIYOS_index> out{.counts = cnt, .items = fun};
    return guarded<get_members<Range>>(mdl, Range{beg, end}, cap, knd, out);
}

BIYOS_status BIYOS_getnconeprmsp(const BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 BIYOS_offset *nnz)
{
    return guarded<n_items<List, Real>>(mdl, List{n, idx}, kParameters, nnz);
}

BIYOS_status BIYOS_getnconeprmrg(const BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 BIYOS_offset *nnz)
{
    return guarded<n_items<Range, Real>>(mdl, Range{beg, end}, kParameters, nnz);
}

BIYOS_status BIYOS_getconeprmsp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                BIYOS_offset cap,
                                BIYOS_offset *prmcnt,
                                BIYOS_real *prm)
{
    const Compressed<BIYOS_real> out{.counts = prmcnt, .items = prm};
    return guarded<get_parameters<List>>(mdl, List{n, idx}, cap, out);
}

BIYOS_status BIYOS_getconeprmrg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                BIYOS_offset cap,
                                BIYOS_offset *prmcnt,
                                BIYOS_real *prm)
{
    const Compressed<BIYOS_real> out{.counts = prmcnt, .items = prm};
    return guarded<get_parameters<Range>>(mdl, Range{beg, end}, cap, out);
}
