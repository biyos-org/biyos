#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/model/tolerances.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"
#include "model/utils/gather.hpp"

#include <cstddef>
#include <span>
#include <vector>

using biyos::ConeKind;
using biyos::Family;
using biyos::Gathered;
using biyos::gathered;
using biyos::Index;
using biyos::item_count;
using biyos::Model;
using biyos::Offset;
using biyos::PartReader;
using biyos::Real;
using biyos::Session;
using biyos::to_offset;
using biyos::to_size;
using biyos::capi::check_capacity;
using biyos::capi::delete_tolerances;
using biyos::capi::get_tolerances;
using biyos::capi::guarded;
using biyos::capi::kinds_of;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::set_tolerances;
using biyos::capi::span_of;
using biyos::capi::starts_of;
using biyos::capi::writable;
using biyos::capi::write_array;
using biyos::capi::write_codes;
using biyos::capi::write_counts;

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

template <typename T> struct Compressed
{
    BIYOS_offset *counts;
    T *items;
};

constexpr PartReader<Index> kMembers = &Model::cone_functions;
constexpr PartReader<Real> kParameters = &Model::cone_parameters;

void add_cones(Session &session, BIYOS_index n, const Given &given, BIYOS_index *start)
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
    model.add_cones(kinds_of<ConeKind>(std::span(given.kinds, size)),
                    starts,
                    span_of(given.functions, to_size(starts.back()), "fun"),
                    parameter_starts,
                    span_of(given.parameters, to_size(parameter_starts.back()), "prm"));
    if (start != nullptr)
    {
        *start = index;
    }
}

void n_cones(const Session &session, BIYOS_index *n)
{
    *require(n, "n") = session.model().n_cones();
}

template <typename Selection, typename T>
void n_items(const Session &session, Selection selection, PartReader<T> part, BIYOS_offset *nnz)
{
    const auto chosen = selected(Family::Cone, session.model().n_of(Family::Cone), selection);
    const std::size_t count = item_count(session.model(), part, chosen);
    *require(nnz, "nnz") = to_offset(count);
}

template <typename Selection>
void get_members(const Session &session,
                 Selection selection,
                 BIYOS_offset capacity,
                 BIYOS_int *kinds,
                 Compressed<BIYOS_index> out)
{
    const Model &model = session.model();
    const auto chosen = selected(Family::Cone, model.n_of(Family::Cone), selection);
    const Gathered<Index> members = gathered(model, kMembers, chosen);
    check_capacity(capacity, members.items.size(), "cap", "members");
    write_codes(gathered(model, &Model::cone_kinds, chosen).items, kinds);
    write_counts(members.starts, out.counts);
    write_array(members.items, out.items);
}

template <typename Selection>
void get_parameters(const Session &session,
                    Selection selection,
                    BIYOS_offset capacity,
                    Compressed<BIYOS_real> out)
{
    const Model &model = session.model();
    const auto chosen = selected(Family::Cone, model.n_of(Family::Cone), selection);
    const Gathered<Real> parameters = gathered(model, kParameters, chosen);
    check_capacity(capacity, parameters.items.size(), "cap", "parameters");
    write_counts(parameters.starts, out.counts);
    write_array(parameters.items, out.items);
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
