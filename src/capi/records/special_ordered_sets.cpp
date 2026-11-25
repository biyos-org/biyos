#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using biyos::Family;
using biyos::Member;
using biyos::Offset;
using biyos::capi::check_capacity;
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::listed;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::starts_of;
using biyos::capi::writable;

namespace
{

struct Outputs
{
    int *kinds;
    std::int64_t *counts;
    int *variables;
    double *weights;
};

template <typename T> void put(T *values, std::size_t k, T value)
{
    if (values != nullptr)
    {
        values[k] = value;
    }
}

std::vector<biyos::SpecialOrderedSetKind> kinds_of(std::span<const int> codes)
{
    std::vector<biyos::SpecialOrderedSetKind> kinds;
    kinds.reserve(codes.size());
    for (const int code : codes)
    {
        kinds.push_back(static_cast<biyos::SpecialOrderedSetKind>(code));
    }
    return kinds;
}

Member member_at(Offset k, Offset start, const int *variables, const double *weights)
{
    const auto at = static_cast<std::size_t>(k);
    const double weight = weights == nullptr ? static_cast<double>(k - start + 1) : weights[at];
    return Member{.variable = variables[at], .weight = weight};
}

std::vector<Member> members_of(std::span<const Offset> starts,
                               const int *variables,
                               const double *weights)
{
    const auto total = static_cast<std::size_t>(starts.back());
    if (total > 0)
    {
        require(variables, "var");
    }
    std::vector<Member> members;
    members.reserve(total);
    for (std::size_t j = 0; j + 1 < starts.size(); ++j)
    {
        for (Offset k = starts[j]; k < starts[j + 1]; ++k)
        {
            members.push_back(member_at(k, starts[j], variables, weights));
        }
    }
    return members;
}

void add_special_ordered_sets(BIYOS_model &model,
                              int n,
                              const int *kinds,
                              const std::int64_t *counts,
                              const int *variables,
                              const double *weights,
                              int *start)
{
    biyos::Model &core = writable(model);
    const std::span<const int> codes = listed(n, kinds, "knd");
    const std::vector<Offset> begins = starts_of(codes.size(), counts);
    core.check_special_ordered_set_room(codes.size(), begins.back());
    const biyos::Index index = core.n_special_ordered_sets();
    core.add_special_ordered_sets(kinds_of(codes), begins, members_of(begins, variables, weights));
    if (start != nullptr)
    {
        *start = index;
    }
}

void n_special_ordered_sets(const BIYOS_model &model, int *n)
{
    *require(n, "n") = model.core.n_special_ordered_sets();
}

std::size_t count_of(const biyos::Model &core, std::span<const biyos::Index> soss)
{
    std::size_t count = 0;
    for (const biyos::Index index : soss)
    {
        count += core.members(index, index + 1).size();
    }
    return count;
}

template <typename Selection>
void n_members(const BIYOS_model &model, Selection selection, std::int64_t *nnz)
{
    const auto chosen = selected(model.core, Family::SpecialOrderedSet, selection);
    const std::size_t count = count_of(model.core, chosen);
    *require(nnz, "nnz") = static_cast<std::int64_t>(count);
}

std::size_t write_members(const biyos::Model &core,
                          biyos::Index sos,
                          std::size_t k,
                          const Outputs &out)
{
    for (const Member &member : core.members(sos, sos + 1))
    {
        put(out.variables, k, member.variable);
        put(out.weights, k, member.weight);
        ++k;
    }
    return k;
}

void write_special_ordered_sets(const biyos::Model &core,
                                std::span<const biyos::Index> soss,
                                const Outputs &out)
{
    std::size_t k = 0;
    for (std::size_t j = 0; j < soss.size(); ++j)
    {
        put(out.kinds, j, static_cast<int>(core.kind_of_special_ordered_set(soss[j])));
        const std::size_t first = k;
        k = write_members(core, soss[j], k, out);
        put(out.counts, j, static_cast<std::int64_t>(k - first));
    }
}

template <typename Selection>
void get_special_ordered_sets(const BIYOS_model &model,
                              Selection selection,
                              std::int64_t capacity,
                              const Outputs &out)
{
    const auto chosen = selected(model.core, Family::SpecialOrderedSet, selection);
    check_capacity(capacity, count_of(model.core, chosen), "cap", "members");
    write_special_ordered_sets(model.core, chosen, out);
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
                                const BIYOS_index *idx,
                                BIYOS_offset *nnz)
{
    return guarded<n_members<List>>(mdl, List{n, idx}, nnz);
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
                            const BIYOS_index *idx,
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
    return guarded<get_special_ordered_sets<List>>(mdl, List{n, idx}, cap, out);
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
