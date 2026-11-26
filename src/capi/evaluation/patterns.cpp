#include "biyos/biyos.h"

#include "capi/evaluation/cache.hpp"
#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/evaluation/evaluation.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <span>

using biyos::Evaluation;
using biyos::Family;
using biyos::Index;
using biyos::Lower;
using biyos::Offset;
using biyos::Pattern;
using biyos::Session;
using biyos::to_offset;
using biyos::to_size;
using biyos::capi::check_capacity;
using biyos::capi::check_known;
using biyos::capi::evaluation_of;
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;

namespace
{

struct Records
{
    std::span<const Offset> starts;
    std::span<const Index> items;
};

struct Jacobian
{
    static constexpr Family kFamily = Family::Function;

    static Records read(const Evaluation &evaluation)
    {
        const Pattern &pattern = evaluation.jacobian_pattern();
        return Records{.starts = pattern.starts, .items = pattern.variables};
    }
};

struct Hessian
{
    static constexpr Family kFamily = Family::Variable;

    static Records read(const Evaluation &evaluation)
    {
        const Lower &lower = evaluation.hessian_pattern();
        return Records{.starts = lower.starts, .items = lower.rows};
    }
};

std::span<const Index> record(const Records &records, Index i)
{
    const auto begin = to_size(records.starts[to_size(i)]);
    const auto end = to_size(records.starts[to_size(i) + 1]);
    return records.items.subspan(begin, end - begin);
}

std::size_t count_of(const Records &records, std::span<const Index> chosen)
{
    std::size_t count = 0;
    for (const Index i : chosen)
    {
        count += record(records, i).size();
    }
    return count;
}

void write(const Records &records,
           std::span<const Index> chosen,
           BIYOS_offset *counts,
           BIYOS_index *items)
{
    std::size_t k = 0;
    for (std::size_t j = 0; j < chosen.size(); ++j)
    {
        const std::span<const Index> found = record(records, chosen[j]);
        if (counts != nullptr)
        {
            counts[j] = to_offset(found.size());
        }
        if (items != nullptr)
        {
            std::copy(found.begin(), found.end(), items + k);
        }
        k += found.size();
    }
}

template <typename Of, typename Selection>
void n_pattern(Session &session, Selection selection, BIYOS_offset *nnz)
{
    BIYOS_offset *out = require(nnz, "nnz");
    const auto chosen = selected(session.model(), Of::kFamily, selection);
    check_known(session.model(), Of::kFamily, chosen);
    const Records records = Of::read(evaluation_of(session));
    *out = to_offset(count_of(records, chosen));
}

template <typename Of, typename Selection>
void get_pattern(Session &session,
                 Selection selection,
                 BIYOS_offset capacity,
                 BIYOS_offset *counts,
                 BIYOS_index *items)
{
    const auto chosen = selected(session.model(), Of::kFamily, selection);
    check_known(session.model(), Of::kFamily, chosen);
    const Records records = Of::read(evaluation_of(session));
    check_capacity(capacity, count_of(records, chosen), "cap", "entries");
    write(records, chosen, counts, items);
}

}

BIYOS_status BIYOS_getngpatsp(BIYOS_model *mdl,
                              BIYOS_index n,
                              const BIYOS_index *idx,
                              BIYOS_offset *nnz)
{
    return guarded<n_pattern<Jacobian, List>>(mdl, List{n, idx}, nnz);
}

BIYOS_status BIYOS_getngpatrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end, BIYOS_offset *nnz)
{
    return guarded<n_pattern<Jacobian, Range>>(mdl, Range{beg, end}, nnz);
}

BIYOS_status BIYOS_getgpatsp(BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *idx,
                             BIYOS_offset cap,
                             BIYOS_offset *cnt,
                             BIYOS_index *var)
{
    return guarded<get_pattern<Jacobian, List>>(mdl, List{n, idx}, cap, cnt, var);
}

BIYOS_status BIYOS_getgpatrg(BIYOS_model *mdl,
                             BIYOS_index beg,
                             BIYOS_index end,
                             BIYOS_offset cap,
                             BIYOS_offset *cnt,
                             BIYOS_index *var)
{
    return guarded<get_pattern<Jacobian, Range>>(mdl, Range{beg, end}, cap, cnt, var);
}

BIYOS_status BIYOS_getnhpatsp(BIYOS_model *mdl,
                              BIYOS_index n,
                              const BIYOS_index *idx,
                              BIYOS_offset *nnz)
{
    return guarded<n_pattern<Hessian, List>>(mdl, List{n, idx}, nnz);
}

BIYOS_status BIYOS_getnhpatrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end, BIYOS_offset *nnz)
{
    return guarded<n_pattern<Hessian, Range>>(mdl, Range{beg, end}, nnz);
}

BIYOS_status BIYOS_gethpatsp(BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *idx,
                             BIYOS_offset cap,
                             BIYOS_offset *cnt,
                             BIYOS_index *var)
{
    return guarded<get_pattern<Hessian, List>>(mdl, List{n, idx}, cap, cnt, var);
}

BIYOS_status BIYOS_gethpatrg(BIYOS_model *mdl,
                             BIYOS_index beg,
                             BIYOS_index end,
                             BIYOS_offset cap,
                             BIYOS_offset *cnt,
                             BIYOS_index *var)
{
    return guarded<get_pattern<Hessian, Range>>(mdl, Range{beg, end}, cap, cnt, var);
}
