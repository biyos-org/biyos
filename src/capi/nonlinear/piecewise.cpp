#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/error.hpp"
#include "core/numbers.hpp"
#include "model/checks.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

using biyos::Index;
using biyos::Offset;
using biyos::to_offset;
using biyos::to_size;
using biyos::capi::check_capacity;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::listed;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::starts_of;
using biyos::capi::writable;

namespace
{

constexpr std::string_view kNoun = "pwl";

struct Outputs
{
    BIYOS_offset *counts;
    BIYOS_real *x;
    BIYOS_real *y;
};

Index held(const biyos::Model &model) noexcept
{
    return static_cast<Index>(model.graph().points().size());
}

std::span<const BIYOS_real> input(const BIYOS_real *values, std::size_t n, std::string_view name)
{
    if (n > 0)
    {
        require(values, name);
    }
    return {values, n};
}

void check_room(const biyos::Model &model, std::size_t added)
{
    if (added > biyos::kRoom - to_size(held(model)))
    {
        throw biyos::Error(biyos::ErrorKind::NoMemory, "model", "no pwl index left");
    }
}

void add_pwls(BIYOS_model &session,
              BIYOS_index n,
              const BIYOS_offset *counts,
              const BIYOS_real *x,
              const BIYOS_real *y,
              BIYOS_index *start)
{
    biyos::Model &model = writable(session);
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(counts, "cnt");
    }
    const std::vector<Offset> begins = starts_of(size, counts);
    check_room(model, size);
    const auto total = to_size(begins.back());
    const std::span<const BIYOS_real> xs = input(x, total, "x");
    const std::span<const BIYOS_real> ys = input(y, total, "y");
    const Offset first = model.add_piecewise_linears(begins, xs, ys);
    if (start != nullptr)
    {
        *start = static_cast<Index>(first);
    }
}

void n_pwls(const BIYOS_model &session, BIYOS_index *n)
{
    *require(n, "n") = held(session.model());
}

std::span<const Index> chosen(const biyos::Model &model, List list)
{
    const std::span<const Index> indices = listed(list.n, list.indices);
    for (const Index index : indices)
    {
        biyos::check_index(kNoun, index, held(model));
    }
    return indices;
}

std::vector<Index> chosen(const biyos::Model &model, Range range)
{
    biyos::check_span(kNoun, range.start, range.stop, held(model));
    std::vector<Index> indices(to_size(range.stop - range.start));
    std::iota(indices.begin(), indices.end(), range.start);
    return indices;
}

std::size_t count_of(const biyos::Model &model, std::span<const Index> pwls)
{
    std::size_t count = 0;
    for (const Index index : pwls)
    {
        count += model.graph().points().piecewise_linear(index).x.size();
    }
    return count;
}

template <typename Selection>
void n_points(const BIYOS_model &session, Selection selection, BIYOS_offset *nnz)
{
    const auto pwls = chosen(session.model(), selection);
    const std::size_t count = count_of(session.model(), pwls);
    *require(nnz, "nnz") = to_offset(count);
}

void copy_to(std::span<const biyos::Real> from, BIYOS_real *to, std::size_t k)
{
    if (to != nullptr)
    {
        std::ranges::copy(from, to + k);
    }
}

void write_pwls(const biyos::Model &model, std::span<const Index> pwls, const Outputs &out)
{
    std::size_t k = 0;
    for (std::size_t j = 0; j < pwls.size(); ++j)
    {
        const auto pwl = model.graph().points().piecewise_linear(pwls[j]);
        copy_to(pwl.x, out.x, k);
        copy_to(pwl.y, out.y, k);
        k += pwl.x.size();
        if (out.counts != nullptr)
        {
            out.counts[j] = to_offset(pwl.x.size());
        }
    }
}

template <typename Selection>
void get_pwls(const BIYOS_model &session,
              Selection selection,
              BIYOS_offset capacity,
              const Outputs &out)
{
    const auto pwls = chosen(session.model(), selection);
    check_capacity(capacity, count_of(session.model(), pwls), "cap", "points");
    write_pwls(session.model(), pwls, out);
}

void add_pwl_node(BIYOS_model &session, BIYOS_index pwl, BIYOS_offset argument, BIYOS_offset *node)
{
    biyos::Model &model = writable(session);
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
                               const BIYOS_index *idx,
                               BIYOS_offset *nnz)
{
    return guarded<n_points<List>>(mdl, List{n, idx}, nnz);
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
                            const BIYOS_index *idx,
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
    return guarded<get_pwls<List>>(mdl, List{n, idx}, cap, out);
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
