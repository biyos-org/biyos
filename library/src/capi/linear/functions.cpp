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
using biyos::Index;
using biyos::Model;
using biyos::Offset;
using biyos::picked;
using biyos::Real;
using biyos::Session;
using biyos::capi::check_known;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::writable;
using biyos::capi::write_array;

namespace
{

void add_empty(Session &session, BIYOS_index n, BIYOS_index *start)
{
    Model &model = writable(session);
    const std::size_t size = length(n);
    model.check_room(size, 0);
    const Index index = model.n_functions();
    const std::vector<Offset> starts(size + 1, 0);
    model.add_functions(std::vector<Real>(size, 0.0), starts, {});
    if (start != nullptr)
    {
        *start = index;
    }
}

void n_functions(const Session &session, BIYOS_index *n)
{
    *require(n, "n") = session.model().n_functions();
}

template <typename Selection> void clear_functions(Session &session, Selection selection)
{
    Model &model = writable(session);
    model.clear_functions(selected(Family::Function, model.n_of(Family::Function), selection));
}

template <typename Selection>
void add_constants(Session &session, Selection selection, const BIYOS_real *constants)
{
    Model &model = writable(session);
    const auto chosen = selected(Family::Function, model.n_of(Family::Function), selection);
    check_known(Family::Function, model.n_of(Family::Function), chosen);
    if (constants != nullptr)
    {
        model.add_constants(chosen, std::span(constants, chosen.size()));
    }
}

template <typename Selection>
void set_constants(Session &session, Selection selection, const BIYOS_real *constants)
{
    Model &model = writable(session);
    const auto chosen = selected(Family::Function, model.n_of(Family::Function), selection);
    check_known(Family::Function, model.n_of(Family::Function), chosen);
    if (constants != nullptr)
    {
        model.set_constants(chosen, std::span(constants, chosen.size()));
    }
}

template <typename Selection>
void get_constants(const Session &session, Selection selection, BIYOS_real *constants)
{
    const auto chosen =
        selected(Family::Function, session.model().n_of(Family::Function), selection);
    write_array(picked(session.model(), &Model::constant, chosen), constants);
}

}

BIYOS_status BIYOS_addfun(BIYOS_model *mdl, BIYOS_index n, BIYOS_index *pos)
{
    return guarded<add_empty>(mdl, n, pos);
}

BIYOS_status BIYOS_getnfun(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_functions>(mdl, n);
}

BIYOS_status BIYOS_addcstsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *fun,
                            const BIYOS_real *cst)
{
    return guarded<add_constants<List>>(mdl, List{n, fun}, cst);
}

BIYOS_status BIYOS_addcstrg(BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index end,
                            const BIYOS_real *cst)
{
    return guarded<add_constants<Range>>(mdl, Range{beg, end}, cst);
}

BIYOS_status BIYOS_setcstsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *fun,
                            const BIYOS_real *cst)
{
    return guarded<set_constants<List>>(mdl, List{n, fun}, cst);
}

BIYOS_status BIYOS_setcstrg(BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index end,
                            const BIYOS_real *cst)
{
    return guarded<set_constants<Range>>(mdl, Range{beg, end}, cst);
}

BIYOS_status BIYOS_getcstsp(const BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *fun,
                            BIYOS_real *cst)
{
    return guarded<get_constants<List>>(mdl, List{n, fun}, cst);
}

BIYOS_status BIYOS_getcstrg(const BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index end,
                            BIYOS_real *cst)
{
    return guarded<get_constants<Range>>(mdl, Range{beg, end}, cst);
}

BIYOS_status BIYOS_clrfunsp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *fun)
{
    return guarded<clear_functions<List>>(mdl, List{n, fun});
}

BIYOS_status BIYOS_clrfunrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<clear_functions<Range>>(mdl, Range{beg, end});
}
