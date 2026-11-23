#include "biyos/biyos.h"

#include "capi/entry.hpp"
#include "capi/selection.hpp"
#include "core/index.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <vector>

using biyos::Family;
using biyos::Model;
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::selected;
using biyos::capi::writable;

namespace
{

template <Family Target, auto Delete, typename Selection>
void delete_records(BIYOS_model &model, Selection selection, int *map)
{
    Model &core = writable(model);
    const std::vector<biyos::Index> renumbered = (core.*Delete)(selected(core, Target, selection));
    if (map != nullptr)
    {
        std::ranges::copy(renumbered, map);
    }
}

template <typename Selection>
void delete_variables(BIYOS_model &model, Selection selection, int *map)
{
    delete_records<Family::Variable, &Model::delete_variables>(model, selection, map);
}

template <typename Selection>
void delete_functions(BIYOS_model &model, Selection selection, int *map)
{
    delete_records<Family::Function, &Model::delete_functions>(model, selection, map);
}

template <typename Selection>
void delete_constraints(BIYOS_model &model, Selection selection, int *map)
{
    delete_records<Family::Constraint, &Model::delete_constraints>(model, selection, map);
}

template <typename Selection>
void delete_objectives(BIYOS_model &model, Selection selection, int *map)
{
    delete_records<Family::Objective, &Model::delete_objectives>(model, selection, map);
}

template <typename Selection>
void delete_indicators(BIYOS_model &model, Selection selection, int *map)
{
    delete_records<Family::Indicator, &Model::delete_indicators>(model, selection, map);
}

template <typename Selection> void delete_sos(BIYOS_model &model, Selection selection, int *map)
{
    delete_records<Family::Sos, &Model::delete_sos>(model, selection, map);
}

template <typename Selection> void delete_cones(BIYOS_model &model, Selection selection, int *map)
{
    delete_records<Family::Cone, &Model::delete_cones>(model, selection, map);
}

template <typename Selection>
void delete_complements(BIYOS_model &model, Selection selection, int *map)
{
    delete_records<Family::Complement, &Model::delete_complements>(model, selection, map);
}

}

BIYOS_status BIYOS_delvarsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *map)
{
    return guarded<delete_variables<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delvarrg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n, BIYOS_index *map)
{
    return guarded<delete_variables<Range>>(mdl, Range{pos, n}, map);
}

BIYOS_status BIYOS_delfunsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *map)
{
    return guarded<delete_functions<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delfunrg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n, BIYOS_index *map)
{
    return guarded<delete_functions<Range>>(mdl, Range{pos, n}, map);
}

BIYOS_status BIYOS_delconsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *map)
{
    return guarded<delete_constraints<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delconrg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n, BIYOS_index *map)
{
    return guarded<delete_constraints<Range>>(mdl, Range{pos, n}, map);
}

BIYOS_status BIYOS_delobjsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *map)
{
    return guarded<delete_objectives<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delobjrg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n, BIYOS_index *map)
{
    return guarded<delete_objectives<Range>>(mdl, Range{pos, n}, map);
}

BIYOS_status BIYOS_delindsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *map)
{
    return guarded<delete_indicators<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delindrg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n, BIYOS_index *map)
{
    return guarded<delete_indicators<Range>>(mdl, Range{pos, n}, map);
}

BIYOS_status BIYOS_delsossp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *map)
{
    return guarded<delete_sos<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delsosrg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n, BIYOS_index *map)
{
    return guarded<delete_sos<Range>>(mdl, Range{pos, n}, map);
}

BIYOS_status BIYOS_delconesp(BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *idx,
                             BIYOS_index *map)
{
    return guarded<delete_cones<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delconerg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n, BIYOS_index *map)
{
    return guarded<delete_cones<Range>>(mdl, Range{pos, n}, map);
}

BIYOS_status BIYOS_delcconsp(BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *idx,
                             BIYOS_index *map)
{
    return guarded<delete_complements<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delcconrg(BIYOS_model *mdl, BIYOS_index pos, BIYOS_index n, BIYOS_index *map)
{
    return guarded<delete_complements<Range>>(mdl, Range{pos, n}, map);
}
