#include "biyos/biyos.h"

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "capi/selection.hpp"
#include "core/index.hpp"
#include "model/compressed.hpp"
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
using biyos::capi::offsets;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::writable;

namespace
{

struct Outputs
{
    int *kinds;
    std::int64_t *starts;
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

std::vector<biyos::SosKind> kinds_of(std::span<const int> codes)
{
    std::vector<biyos::SosKind> kinds;
    kinds.reserve(codes.size());
    for (const int code : codes)
    {
        kinds.push_back(static_cast<biyos::SosKind>(code));
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
        require(variables, "varidx");
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

void add_sos(BIYOS_model &model,
             int n,
             const int *kinds,
             const std::int64_t *starts,
             const int *variables,
             const double *weights,
             int *start)
{
    biyos::Model &core = writable(model);
    const std::span<const int> codes = listed(n, kinds, "knd");
    const std::vector<Offset> begins = offsets(codes.size(), starts);
    biyos::check_starts(begins);
    core.check_sos_room(codes.size(), begins.back());
    const biyos::Index index = core.n_sos();
    core.add_sos(kinds_of(codes), begins, members_of(begins, variables, weights));
    if (start != nullptr)
    {
        *start = index;
    }
}

void n_sos(const BIYOS_model &model, int *n)
{
    *require(n, "n") = model.core.n_sos();
}

std::size_t count_of(const biyos::Model &core, std::span<const biyos::Index> sos)
{
    std::size_t count = 0;
    for (const biyos::Index index : sos)
    {
        count += core.members(index, 1).size();
    }
    return count;
}

template <typename Selection>
void n_members(const BIYOS_model &model, Selection selection, std::int64_t *nnz)
{
    const auto chosen = selected(model.core, Family::Sos, selection);
    const std::size_t count = count_of(model.core, chosen);
    *require(nnz, "nnz") = static_cast<std::int64_t>(count);
}

std::size_t write_members(const biyos::Model &core,
                          biyos::Index sos,
                          std::size_t k,
                          const Outputs &out)
{
    for (const Member &member : core.members(sos, 1))
    {
        put(out.variables, k, member.variable);
        put(out.weights, k, member.weight);
        ++k;
    }
    return k;
}

void write_sos(const biyos::Model &core, std::span<const biyos::Index> sos, const Outputs &out)
{
    std::size_t k = 0;
    for (std::size_t j = 0; j < sos.size(); ++j)
    {
        put(out.kinds, j, static_cast<int>(core.kind_of_sos(sos[j])));
        put(out.starts, j, static_cast<std::int64_t>(k));
        k = write_members(core, sos[j], k, out);
    }
    put(out.starts, sos.size(), static_cast<std::int64_t>(k));
}

template <typename Selection>
void get_sos(const BIYOS_model &model,
             Selection selection,
             std::int64_t capacity,
             const Outputs &out)
{
    const auto chosen = selected(model.core, Family::Sos, selection);
    check_capacity(capacity, count_of(model.core, chosen), "members");
    write_sos(model.core, chosen, out);
}

}

BIYOS_status BIYOS_addsos(BIYOS_model *mdl,
                          BIYOS_index n,
                          const BIYOS_int *knd,
                          const BIYOS_offset *off,
                          const BIYOS_index *varidx,
                          const BIYOS_real *coef,
                          BIYOS_index *pos)
{
    return guarded<add_sos>(mdl, n, knd, off, varidx, coef, pos);
}

BIYOS_status BIYOS_getnsos(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_sos>(mdl, n);
}

BIYOS_status BIYOS_getnsosvarsp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                BIYOS_offset *nnz)
{
    return guarded<n_members<List>>(mdl, List{n, idx}, nnz);
}

BIYOS_status BIYOS_getnsosvarrg(const BIYOS_model *mdl,
                                BIYOS_index pos,
                                BIYOS_index n,
                                BIYOS_offset *nnz)
{
    return guarded<n_members<Range>>(mdl, Range{pos, n}, nnz);
}

BIYOS_status BIYOS_getsossp(const BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_offset cap,
                            BIYOS_int *knd,
                            BIYOS_offset *off,
                            BIYOS_index *varidx,
                            BIYOS_real *coef)
{
    const Outputs out{
        .kinds = knd,
        .starts = off,
        .variables = varidx,
        .weights = coef,
    };
    return guarded<get_sos<List>>(mdl, List{n, idx}, cap, out);
}

BIYOS_status BIYOS_getsosrg(const BIYOS_model *mdl,
                            BIYOS_index pos,
                            BIYOS_index n,
                            BIYOS_offset cap,
                            BIYOS_int *knd,
                            BIYOS_offset *off,
                            BIYOS_index *varidx,
                            BIYOS_real *coef)
{
    const Outputs out{
        .kinds = knd,
        .starts = off,
        .variables = varidx,
        .weights = coef,
    };
    return guarded<get_sos<Range>>(mdl, Range{pos, n}, cap, out);
}
