#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <vector>

using biyos::Family;
using biyos::to_index;
using biyos::capi::entry_or;
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::listed;
using biyos::capi::picked;
using biyos::capi::put;
using biyos::capi::Range;
using biyos::capi::require;
using biyos::capi::selected;
using biyos::capi::writable;

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

biyos::Objective given(biyos::Objective objective, std::size_t k, const Fields &fields)
{
    const BIYOS_int sense = entry_or(fields.senses, k, static_cast<BIYOS_int>(objective.sense));
    objective.sense = static_cast<biyos::ObjectiveSense>(sense);
    objective.priority = entry_or(fields.priorities, k, objective.priority);
    objective.weight = entry_or(fields.weights, k, objective.weight);
    objective.absolute = entry_or(fields.absolute, k, objective.absolute);
    objective.relative = entry_or(fields.relative, k, objective.relative);
    return objective;
}

std::vector<biyos::Objective> new_objectives(std::span<const biyos::Index> functions,
                                             biyos::Index start,
                                             const Fields &fields)
{
    std::vector<biyos::Objective> added;
    added.reserve(functions.size());
    for (std::size_t k = 0; k < functions.size(); ++k)
    {
        const biyos::Index index = start + to_index(k);
        added.push_back(given(biyos::default_objective(functions[k], index), k, fields));
    }
    return added;
}

void add_objectives(BIYOS_model &model,
                    BIYOS_index n,
                    const BIYOS_index *functions,
                    const BIYOS_int *senses,
                    const BIYOS_int *priorities,
                    const BIYOS_real *weights,
                    BIYOS_index *start)
{
    biyos::Model &core = writable(model);
    const std::span<const biyos::Index> owners = listed(n, functions, "fun");
    core.check_objective_room(owners.size());
    const Fields fields{
        .senses = senses,
        .priorities = priorities,
        .weights = weights,
    };
    const biyos::Index index = core.n_objectives();
    core.add_objectives(new_objectives(owners, index, fields));
    if (start != nullptr)
    {
        *start = index;
    }
}

template <typename Selection>
void set_objectives(BIYOS_model &model, Selection selection, const Fields &fields)
{
    biyos::Model &core = writable(model);
    const auto changed = selected(core, Family::Objective, selection);
    std::vector<biyos::Objective> objectives;
    objectives.reserve(changed.size());
    for (std::size_t k = 0; k < changed.size(); ++k)
    {
        objectives.push_back(given(core.objective(changed[k]), k, fields));
    }
    core.set_objectives(changed, objectives);
}

void n_objectives(const BIYOS_model &model, BIYOS_index *n)
{
    *require(n, "n") = model.model().n_objectives();
}

void copy_out(const biyos::Objective &objective, std::size_t k, const Outputs &outputs)
{
    put(outputs.functions, k, objective.function);
    put(outputs.senses, k, static_cast<BIYOS_int>(objective.sense));
    put(outputs.priorities, k, objective.priority);
    put(outputs.weights, k, objective.weight);
    put(outputs.absolute, k, objective.absolute);
    put(outputs.relative, k, objective.relative);
}

template <typename Selection>
void get_chosen(const BIYOS_model &model, Selection selection, const Outputs &outputs)
{
    const auto chosen = selected(model.model(), Family::Objective, selection);
    const std::vector<biyos::Objective> objectives =
        picked(model.model(), &biyos::Model::objective, chosen);
    for (std::size_t k = 0; k < objectives.size(); ++k)
    {
        copy_out(objectives[k], k, outputs);
    }
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
