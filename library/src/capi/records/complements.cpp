#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/model/tolerances.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "model/model.hpp"
#include "model/utils/gather.hpp"

#include <cstddef>
#include <vector>

using biyos::Complement;
using biyos::Family;
using biyos::Index;
using biyos::kInfinity;
using biyos::Model;
using biyos::picked;
using biyos::Session;
using biyos::capi::delete_tolerances;
using biyos::capi::get_tolerances;
using biyos::capi::guarded;
using biyos::capi::length;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::read_field;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::set_tolerances;
using biyos::capi::writable;
using biyos::capi::write_field;

namespace
{
struct Fields
{
    const BIYOS_index *functions = nullptr;
    const BIYOS_index *multipliers = nullptr;
    const BIYOS_real *lower = nullptr;
    const BIYOS_real *upper = nullptr;
};

struct Outputs
{
    BIYOS_index *functions = nullptr;
    BIYOS_index *multipliers = nullptr;
    BIYOS_real *lower = nullptr;
    BIYOS_real *upper = nullptr;
};

void add_complements(Session &session, BIYOS_index n, const Fields &fields, BIYOS_index *start)
{
    Model &model = writable(session);
    const std::size_t size = length(n);
    if (size > 0)
    {
        require(fields.functions, "fun1");
        require(fields.multipliers, "fun2");
    }
    const Complement nonnegative{
        .function = 0,
        .lower = 0.0,
        .upper = kInfinity,
        .multiplier = 0,
    };
    std::vector<Complement> added(size, nonnegative);
    read_field(added, &Complement::function, fields.functions);
    read_field(added, &Complement::multiplier, fields.multipliers);
    read_field(added, &Complement::lower, fields.lower);
    read_field(added, &Complement::upper, fields.upper);
    const Index index = model.n_complements();
    model.add_complements(added);
    if (start != nullptr)
    {
        *start = index;
    }
}

template <typename Selection>
void set_bounds(Session &session, Selection selection, const Fields &fields)
{
    Model &model = writable(session);
    const auto changed = selected(Family::Complement, model.n_of(Family::Complement), selection);
    std::vector<Complement> complements = picked(model, &Model::complement, changed);
    read_field(complements, &Complement::function, fields.functions);
    read_field(complements, &Complement::multiplier, fields.multipliers);
    read_field(complements, &Complement::lower, fields.lower);
    read_field(complements, &Complement::upper, fields.upper);
    model.set_complements(changed, complements);
}

void n_complements(const Session &session, BIYOS_index *n)
{
    *require(n, "n") = session.model().n_complements();
}

template <typename Selection>
void get_chosen(const Session &session, Selection selection, const Outputs &outputs)
{
    const auto chosen =
        selected(Family::Complement, session.model().n_of(Family::Complement), selection);
    const std::vector<Complement> complements = picked(session.model(), &Model::complement, chosen);
    write_field(complements, &Complement::function, outputs.functions);
    write_field(complements, &Complement::multiplier, outputs.multipliers);
    write_field(complements, &Complement::lower, outputs.lower);
    write_field(complements, &Complement::upper, outputs.upper);
}

}

BIYOS_status BIYOS_addccon(BIYOS_model *mdl,
                           BIYOS_index n,
                           const BIYOS_index *fun1,
                           const BIYOS_index *fun2,
                           const BIYOS_real *lb,
                           const BIYOS_real *ub,
                           BIYOS_index *pos)
{
    const Fields fields{
        .functions = fun1,
        .multipliers = fun2,
        .lower = lb,
        .upper = ub,
    };
    return guarded<add_complements>(mdl, n, fields, pos);
}

BIYOS_status BIYOS_setcconbndsp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *ccon,
                                const BIYOS_real *lb,
                                const BIYOS_real *ub)
{
    return guarded<set_bounds<List>>(mdl, List{n, ccon}, Fields{.lower = lb, .upper = ub});
}

BIYOS_status BIYOS_setcconbndrg(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const BIYOS_real *lb,
                                const BIYOS_real *ub)
{
    return guarded<set_bounds<Range>>(mdl, Range{beg, end}, Fields{.lower = lb, .upper = ub});
}

BIYOS_status BIYOS_getcconbndsp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *ccon,
                                BIYOS_real *lb,
                                BIYOS_real *ub)
{
    return guarded<get_chosen<List>>(mdl, List{n, ccon}, Outputs{.lower = lb, .upper = ub});
}

BIYOS_status BIYOS_getcconbndrg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                BIYOS_real *lb,
                                BIYOS_real *ub)
{
    return guarded<get_chosen<Range>>(mdl, Range{beg, end}, Outputs{.lower = lb, .upper = ub});
}

BIYOS_status BIYOS_setccontolsp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *ccon,
                                const BIYOS_real *atol,
                                const BIYOS_real *rtol)
{
    return guarded<set_tolerances<List>>(mdl, Family::Complement, List{n, ccon}, atol, rtol);
}

BIYOS_status BIYOS_setccontolrg(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const BIYOS_real *atol,
                                const BIYOS_real *rtol)
{
    return guarded<set_tolerances<Range>>(mdl, Family::Complement, Range{beg, end}, atol, rtol);
}

BIYOS_status BIYOS_getccontolsp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *ccon,
                                BIYOS_real *atol,
                                BIYOS_real *rtol)
{
    return guarded<get_tolerances<List>>(mdl, Family::Complement, List{n, ccon}, atol, rtol);
}

BIYOS_status BIYOS_getccontolrg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                BIYOS_real *atol,
                                BIYOS_real *rtol)
{
    return guarded<get_tolerances<Range>>(mdl, Family::Complement, Range{beg, end}, atol, rtol);
}

BIYOS_status BIYOS_delccontolsp(BIYOS_model *mdl, BIYOS_index n, const BIYOS_index *ccon)
{
    return guarded<delete_tolerances<List>>(mdl, Family::Complement, List{n, ccon});
}

BIYOS_status BIYOS_delccontolrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end)
{
    return guarded<delete_tolerances<Range>>(mdl, Family::Complement, Range{beg, end});
}

BIYOS_status BIYOS_getnccon(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_complements>(mdl, n);
}

BIYOS_status BIYOS_getcconsp(const BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *ccon,
                             BIYOS_index *fun1,
                             BIYOS_index *fun2,
                             BIYOS_real *lb,
                             BIYOS_real *ub)
{
    const Outputs outputs{
        .functions = fun1,
        .multipliers = fun2,
        .lower = lb,
        .upper = ub,
    };
    return guarded<get_chosen<List>>(mdl, List{n, ccon}, outputs);
}

BIYOS_status BIYOS_getcconrg(const BIYOS_model *mdl,
                             BIYOS_index beg,
                             BIYOS_index end,
                             BIYOS_index *fun1,
                             BIYOS_index *fun2,
                             BIYOS_real *lb,
                             BIYOS_real *ub)
{
    const Outputs outputs{
        .functions = fun1,
        .multipliers = fun2,
        .lower = lb,
        .upper = ub,
    };
    return guarded<get_chosen<Range>>(mdl, Range{beg, end}, outputs);
}
