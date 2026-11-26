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

using biyos::default_objective;
using biyos::Family;
using biyos::Index;
using biyos::Model;
using biyos::Objective;
using biyos::picked;
using biyos::Session;
using biyos::to_index;
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::listed;
using biyos::capi::Range;
using biyos::capi::read_field;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::writable;
using biyos::capi::write_field;

namespace
{

struct Fields
{
    const BIYOS_int *senses = nullptr;
    const BIYOS_int *priorities = nullptr;
    const BIYOS_real *weights = nullptr;
    const BIYOS_real *absolute = nullptr;
    const BIYOS_real *relative = nullptr;
};

struct Outputs
{
    BIYOS_index *functions = nullptr;
    BIYOS_int *senses = nullptr;
    BIYOS_int *priorities = nullptr;
    BIYOS_real *weights = nullptr;
    BIYOS_real *absolute = nullptr;
    BIYOS_real *relative = nullptr;
};

std::vector<Objective> default_objectives(std::span<const Index> functions, Index start)
{
    std::vector<Objective> added;
    added.reserve(functions.size());
    for (std::size_t k = 0; k < functions.size(); ++k)
    {
        added.push_back(default_objective(functions[k], start + to_index(k)));
    }
    return added;
}

void add_objectives(Session &session,
                    BIYOS_index n,
                    const BIYOS_index *functions,
                    const BIYOS_int *senses,
                    const BIYOS_int *priorities,
                    const BIYOS_real *weights,
                    BIYOS_index *start)
{
    Model &model = writable(session);
    const std::span<const Index> owners = listed(n, functions, "fun");
    model.check_objective_room(owners.size());
    const Index index = model.n_objectives();
    std::vector<Objective> added = default_objectives(owners, index);
    read_field(added, &Objective::sense, senses);
    read_field(added, &Objective::priority, priorities);
    read_field(added, &Objective::weight, weights);
    model.add_objectives(added);
    if (start != nullptr)
    {
        *start = index;
    }
}

template <typename Selection>
void set_objectives(Session &session, Selection selection, const Fields &fields)
{
    Model &model = writable(session);
    const auto changed = selected(model, Family::Objective, selection);
    std::vector<Objective> objectives = picked(model, &Model::objective, changed);
    read_field(objectives, &Objective::sense, fields.senses);
    read_field(objectives, &Objective::priority, fields.priorities);
    read_field(objectives, &Objective::weight, fields.weights);
    read_field(objectives, &Objective::absolute, fields.absolute);
    read_field(objectives, &Objective::relative, fields.relative);
    model.set_objectives(changed, objectives);
}

void n_objectives(const Session &session, BIYOS_index *n)
{
    *require(n, "n") = session.model().n_objectives();
}

template <typename Selection>
void get_chosen(const Session &session, Selection selection, const Outputs &outputs)
{
    const auto chosen = selected(session.model(), Family::Objective, selection);
    const std::vector<Objective> objectives = picked(session.model(), &Model::objective, chosen);
    write_field(objectives, &Objective::function, outputs.functions);
    write_field(objectives, &Objective::sense, outputs.senses);
    write_field(objectives, &Objective::priority, outputs.priorities);
    write_field(objectives, &Objective::weight, outputs.weights);
    write_field(objectives, &Objective::absolute, outputs.absolute);
    write_field(objectives, &Objective::relative, outputs.relative);
}

}

BIYOS_status BIYOS_addobj(BIYOS_model *mdl,
                          BIYOS_index n,
                          const BIYOS_index *fun,
                          const BIYOS_int *sense,
                          const BIYOS_int *prio,
                          const BIYOS_real *coef,
                          BIYOS_index *pos)
{
    return guarded<add_objectives>(mdl, n, fun, sense, prio, coef, pos);
}

BIYOS_status BIYOS_setobjsensesp(BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 const BIYOS_int *sense)
{
    return guarded<set_objectives<List>>(mdl, List{n, idx}, Fields{.senses = sense});
}

BIYOS_status BIYOS_setobjsenserg(BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 const BIYOS_int *sense)
{
    return guarded<set_objectives<Range>>(mdl, Range{beg, end}, Fields{.senses = sense});
}

BIYOS_status BIYOS_getobjsensesp(const BIYOS_model *mdl,
                                 BIYOS_index n,
                                 const BIYOS_index *idx,
                                 BIYOS_int *sense)
{
    return guarded<get_chosen<List>>(mdl, List{n, idx}, Outputs{.senses = sense});
}

BIYOS_status BIYOS_getobjsenserg(const BIYOS_model *mdl,
                                 BIYOS_index beg,
                                 BIYOS_index end,
                                 BIYOS_int *sense)
{
    return guarded<get_chosen<Range>>(mdl, Range{beg, end}, Outputs{.senses = sense});
}

BIYOS_status BIYOS_setobjpriosp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const BIYOS_int *prio)
{
    return guarded<set_objectives<List>>(mdl, List{n, idx}, Fields{.priorities = prio});
}

BIYOS_status BIYOS_setobjpriorg(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const BIYOS_int *prio)
{
    return guarded<set_objectives<Range>>(mdl, Range{beg, end}, Fields{.priorities = prio});
}

BIYOS_status BIYOS_getobjpriosp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                BIYOS_int *prio)
{
    return guarded<get_chosen<List>>(mdl, List{n, idx}, Outputs{.priorities = prio});
}

BIYOS_status BIYOS_getobjpriorg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                BIYOS_int *prio)
{
    return guarded<get_chosen<Range>>(mdl, Range{beg, end}, Outputs{.priorities = prio});
}

BIYOS_status BIYOS_setobjcoefsp(BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                const BIYOS_real *coef)
{
    return guarded<set_objectives<List>>(mdl, List{n, idx}, Fields{.weights = coef});
}

BIYOS_status BIYOS_setobjcoefrg(BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                const BIYOS_real *coef)
{
    return guarded<set_objectives<Range>>(mdl, Range{beg, end}, Fields{.weights = coef});
}

BIYOS_status BIYOS_getobjcoefsp(const BIYOS_model *mdl,
                                BIYOS_index n,
                                const BIYOS_index *idx,
                                BIYOS_real *coef)
{
    return guarded<get_chosen<List>>(mdl, List{n, idx}, Outputs{.weights = coef});
}

BIYOS_status BIYOS_getobjcoefrg(const BIYOS_model *mdl,
                                BIYOS_index beg,
                                BIYOS_index end,
                                BIYOS_real *coef)
{
    return guarded<get_chosen<Range>>(mdl, Range{beg, end}, Outputs{.weights = coef});
}

BIYOS_status BIYOS_setobjtolsp(BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               const BIYOS_real *atol,
                               const BIYOS_real *rtol)
{
    return guarded<set_objectives<List>>(mdl,
                                         List{n, idx},
                                         Fields{.absolute = atol, .relative = rtol});
}

BIYOS_status BIYOS_setobjtolrg(BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               const BIYOS_real *atol,
                               const BIYOS_real *rtol)
{
    return guarded<set_objectives<Range>>(mdl,
                                          Range{beg, end},
                                          Fields{.absolute = atol, .relative = rtol});
}

BIYOS_status BIYOS_getobjtolsp(const BIYOS_model *mdl,
                               BIYOS_index n,
                               const BIYOS_index *idx,
                               BIYOS_real *atol,
                               BIYOS_real *rtol)
{
    return guarded<get_chosen<List>>(mdl,
                                     List{n, idx},
                                     Outputs{.absolute = atol, .relative = rtol});
}

BIYOS_status BIYOS_getobjtolrg(const BIYOS_model *mdl,
                               BIYOS_index beg,
                               BIYOS_index end,
                               BIYOS_real *atol,
                               BIYOS_real *rtol)
{
    return guarded<get_chosen<Range>>(mdl,
                                      Range{beg, end},
                                      Outputs{.absolute = atol, .relative = rtol});
}

BIYOS_status BIYOS_getnobj(const BIYOS_model *mdl, BIYOS_index *n)
{
    return guarded<n_objectives>(mdl, n);
}

BIYOS_status BIYOS_getobjsp(const BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *fun,
                            BIYOS_int *sense,
                            BIYOS_int *prio,
                            BIYOS_real *coef)
{
    const Outputs outputs{.functions = fun, .senses = sense, .priorities = prio, .weights = coef};
    return guarded<get_chosen<List>>(mdl, List{n, idx}, outputs);
}

BIYOS_status BIYOS_getobjrg(const BIYOS_model *mdl,
                            BIYOS_index beg,
                            BIYOS_index end,
                            BIYOS_index *fun,
                            BIYOS_int *sense,
                            BIYOS_int *prio,
                            BIYOS_real *coef)
{
    const Outputs outputs{.functions = fun, .senses = sense, .priorities = prio, .weights = coef};
    return guarded<get_chosen<Range>>(mdl, Range{beg, end}, outputs);
}
