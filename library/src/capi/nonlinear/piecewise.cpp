#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"
#include "model/utils/gather.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

using biyos::Error;
using biyos::ErrorKind;
using biyos::Gathered;
using biyos::gathered;
using biyos::Index;
using biyos::kRoom;
using biyos::Model;
using biyos::Offset;
using biyos::Real;
using biyos::Session;
using biyos::to_offset;
using biyos::to_size;
using biyos::capi::check_capacity;
using biyos::capi::check_known;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::span_of;
using biyos::capi::starts_of;
using biyos::capi::writable;
using biyos::capi::write_array;
using biyos::capi::write_counts;

namespace
{

constexpr std::string_view kNoun = "pwl";

struct Outputs
{
    BIYOS_offset *counts;
    BIYOS_real *x;
    BIYOS_real *y;
};

Index held(const Model &model) noexcept
{
    return static_cast<Index>(model.graph().points().size());
}

void check_room(const Model &model, std::size_t added)
{
    if (added > kRoom - to_size(held(model)))
    {
        throw Error(ErrorKind::NoMemory, "model", "no pwl index left");
    }
}

void add_pwls(Session &session,
              BIYOS_index n,
              const BIYOS_offset *counts,
              const BIYOS_real *x,
              const BIYOS_real *y,
              BIYOS_index *start)
{
    Model &model = writable(session);
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(counts, "cnt");
    }
    const std::vector<Offset> begins = starts_of(size, counts);
    check_room(model, size);
    const auto total = to_size(begins.back());
    const std::span<const Real> xs = span_of(x, total, "x");
    const std::span<const Real> ys = span_of(y, total, "y");
    const Offset first = model.add_piecewise_linears(begins, xs, ys);
    if (start != nullptr)
    {
        *start = static_cast<Index>(first);
    }
}

void n_pwls(const Session &session, BIYOS_index *n)
{
    *require(n, "n") = held(session.model());
}

std::size_t count_of(const Model &model, std::span<const Index> pwls)
{
    std::size_t count = 0;
    for (const Index index : pwls)
    {
        count += model.graph().points().piecewise_linear(index).x.size();
    }
    return count;
}

template <typename Selection>
void n_points(const Session &session, Selection selection, BIYOS_offset *nnz)
{
    const Model &model = session.model();
    const auto pwls = selected(kNoun, held(model), selection);
    check_known(kNoun, held(model), pwls);
    const std::size_t count = count_of(model, pwls);
    *require(nnz, "nnz") = to_offset(count);
}

template <typename Selection>
void get_pwls(const Session &session,
              Selection selection,
              BIYOS_offset capacity,
              const Outputs &out)
{
    const Model &model = session.model();
    const auto pwls = selected(kNoun, held(model), selection);
    check_known(kNoun, held(model), pwls);
    const auto &points = model.graph().points();
    const Gathered<Real> xs =
        gathered(pwls, [&](Index pwl) { return points.piecewise_linear(pwl).x; });
    const Gathered<Real> ys =
        gathered(pwls, [&](Index pwl) { return points.piecewise_linear(pwl).y; });
    check_capacity(capacity, xs.items.size(), "cap", "points");
    write_counts(xs.starts, out.counts);
    write_array(xs.items, out.x);
    write_array(ys.items, out.y);
}

void add_pwl_node(Session &session, BIYOS_index pwl, BIYOS_offset argument, BIYOS_offset *node)
{
    Model &model = writable(session);
    const Offset made = model.add_piecewise(pwl, argument);
    if (node != nullptr)
    {
        *node = made;
    }
}

}

BIYOS_status BIYOS_addpwl(BIYOS_model *mdl,
                          BIYOS_index n,
                          const BIYOS_offset *cnt,
                          const BIYOS_real *x,
                          const BIYOS_real *y,
                          BIYOS_index *pos)
{
    return guarded<add_pwls>(mdl, n, cnt, x, y, pos);
}

BIYOS_status BIYOS_getnpwl(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_pwls>(mdl, n);
}

BIYOS_status BIYOS_getnpwlptsp(const BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *pwl,
                               BIYOS_offset *nnz)
{
    return guarded<n_points<List>>(mdl, List{n, pwl}, nnz);
}

BIYOS_status BIYOS_getnpwlptrg(const BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               BIYOS_offset *nnz)
{
    return guarded<n_points<Range>>(mdl, Range{beg, end}, nnz);
}

BIYOS_status BIYOS_getpwlsp(const BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *pwl,
                            BIYOS_offset cap,
                            BIYOS_offset *cnt,
                            BIYOS_real *x,
                            BIYOS_real *y)
{
    const Outputs out{
        .counts = cnt,
        .x = x,
        .y = y,
    };
    return guarded<get_pwls<List>>(mdl, List{n, pwl}, cap, out);
}

BIYOS_status BIYOS_getpwlrg(const BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index end,
                            BIYOS_offset cap,
                            BIYOS_offset *cnt,
                            BIYOS_real *x,
                            BIYOS_real *y)
{
    const Outputs out{
        .counts = cnt,
        .x = x,
        .y = y,
    };
    return guarded<get_pwls<Range>>(mdl, Range{beg, end}, cap, out);
}

BIYOS_status BIYOS_addpwlnode(BIYOS_model *mdl,
                              BIYOS_index pwl,
                              BIYOS_offset arg,
                              BIYOS_offset *node)
{
    return guarded<add_pwl_node>(mdl, pwl, arg, node);
}
