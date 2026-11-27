#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"
#include "model/utils/gather.hpp"

#include <cstddef>
#include <span>
#include <vector>

using biyos::Family;
using biyos::Gathered;
using biyos::gathered;
using biyos::Index;
using biyos::item_count;
using biyos::Member;
using biyos::Model;
using biyos::Offset;
using biyos::picked;
using biyos::Real;
using biyos::Session;
using biyos::SpecialOrderedSetKind;
using biyos::to_offset;
using biyos::to_size;
using biyos::capi::check_capacity;
using biyos::capi::given;
using biyos::capi::guarded;
using biyos::capi::kinds_of;
using biyos::capi::List;
using biyos::capi::listed;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::span_of;
using biyos::capi::starts_of;
using biyos::capi::writable;
using biyos::capi::write_codes;
using biyos::capi::write_counts;
using biyos::capi::write_field;

namespace
{

struct Outputs
{
    BIYOS_int *kinds;
    BIYOS_offset *counts;
    BIYOS_index *variables;
    BIYOS_real *weights;
};

Member member_at(Offset k,
                 Offset start,
                 std::span<const Index> variables,
                 std::span<const Real> weights)
{
    const auto at = to_size(k);
    const Real weight = weights.empty() ? static_cast<Real>(k - start + 1) : weights[at];
    return Member{.variable = variables[at], .weight = weight};
}

std::vector<Member> members_of(std::span<const Offset> starts,
                               std::span<const Index> variables,
                               std::span<const Real> weights)
{
    std::vector<Member> members;
    members.reserve(variables.size());
    for (std::size_t j = 0; j + 1 < starts.size(); ++j)
    {
        for (Offset k = starts[j]; k < starts[j + 1]; ++k)
        {
            members.push_back(member_at(k, starts[j], variables, weights));
        }
    }
    return members;
}

void add_special_ordered_sets(Session &session,
                              BIYOS_index n,
                              const BIYOS_int *kinds,
                              const BIYOS_offset *counts,
                              const BIYOS_index *variables,
                              const BIYOS_real *weights,
                              BIYOS_index *start)
{
    Model &model = writable(session);
    const std::span<const BIYOS_int> codes = listed(n, kinds, "knd");
    const std::vector<Offset> begins = starts_of(codes.size(), counts);
    model.check_special_ordered_set_room(codes.size(), begins.back());
    const Index index = model.n_special_ordered_sets();
    const auto total = to_size(begins.back());
    const std::vector<Member> members =
        members_of(begins, span_of(variables, total, "var"), given(weights, total));
    model.add_special_ordered_sets(kinds_of<SpecialOrderedSetKind>(codes), begins, members);
    if (start != nullptr)
    {
        *start = index;
    }
}

void n_special_ordered_sets(const Session &session, BIYOS_index *n)
{
    *require(n, "n") = session.model().n_special_ordered_sets();
}

template <typename Selection>
void n_members(const Session &session, Selection selection, BIYOS_offset *nnz)
{
    const auto chosen = selected(Family::SpecialOrderedSet,
                                 session.model().n_of(Family::SpecialOrderedSet),
                                 selection);
    const std::size_t count = item_count(session.model(), &Model::members, chosen);
    *require(nnz, "nnz") = to_offset(count);
}

template <typename Selection>
void get_special_ordered_sets(const Session &session,
                              Selection selection,
                              BIYOS_offset capacity,
                              const Outputs &out)
{
    const Model &model = session.model();
    const auto chosen =
        selected(Family::SpecialOrderedSet, model.n_of(Family::SpecialOrderedSet), selection);
    const Gathered<Member> sets = gathered(model, &Model::members, chosen);
    check_capacity(capacity, sets.items.size(), "cap", "members");
    write_codes(picked(model, &Model::kind_of_special_ordered_set, chosen), out.kinds);
    write_counts(sets.starts, out.counts);
    write_field(sets.items, &Member::variable, out.variables);
    write_field(sets.items, &Member::weight, out.weights);
}

}

BIYOS_status BIYOS_addsos(BIYOS_model *mdl,
                          BIYOS_index n,
                          const BIYOS_int *knd,
                          const BIYOS_offset *cnt,
                          const BIYOS_index *var,
                          const BIYOS_real *coef,
                          BIYOS_index *pos)
{
    return guarded<add_special_ordered_sets>(mdl, n, knd, cnt, var, coef, pos);
}

BIYOS_status BIYOS_getnsos(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_special_ordered_sets>(mdl, n);
}

BIYOS_status BIYOS_getnsosvarsp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *sos,
                                BIYOS_offset *nnz)
{
    return guarded<n_members<List>>(mdl, List{n, sos}, nnz);
}

BIYOS_status BIYOS_getnsosvarrg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                BIYOS_offset *nnz)
{
    return guarded<n_members<Range>>(mdl, Range{beg, end}, nnz);
}

BIYOS_status BIYOS_getsossp(const BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *sos,
                            BIYOS_offset cap,
                            BIYOS_int *knd,
                            BIYOS_offset *cnt,
                            BIYOS_index *var,
                            BIYOS_real *coef)
{
    const Outputs out{
        .kinds = knd,
        .counts = cnt,
        .variables = var,
        .weights = coef,
    };
    return guarded<get_special_ordered_sets<List>>(mdl, List{n, sos}, cap, out);
}

BIYOS_status BIYOS_getsosrg(const BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index end,
                            BIYOS_offset cap,
                            BIYOS_int *knd,
                            BIYOS_offset *cnt,
                            BIYOS_index *var,
                            BIYOS_real *coef)
{
    const Outputs out{
        .kinds = knd,
        .counts = cnt,
        .variables = var,
        .weights = coef,
    };
    return guarded<get_special_ordered_sets<Range>>(mdl, Range{beg, end}, cap, out);
}
