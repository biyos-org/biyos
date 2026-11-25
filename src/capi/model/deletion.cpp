#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "core/timing/timer.hpp"
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
void delete_records(BIYOS_model &session, Selection selection, BIYOS_index *map)
{
    Model &model = writable(session);
    const auto chosen = selected(model, Target, selection);
    const biyos::EntryTimer timer(session.env(), biyos::kModule, "delete");
    const std::vector<biyos::Index> renumbered = (model.*Delete)(chosen);
    if (map != nullptr)
    {
        std::ranges::copy(renumbered, map);
    }
}

template <typename Selection>
void delete_variables(BIYOS_model &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Variable, &Model::delete_variables>(session, selection, map);
}

template <typename Selection>
void delete_functions(BIYOS_model &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Function, &Model::delete_functions>(session, selection, map);
}

template <typename Selection>
void delete_constraints(BIYOS_model &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Constraint, &Model::delete_constraints>(session, selection, map);
}

template <typename Selection>
void delete_objectives(BIYOS_model &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Objective, &Model::delete_objectives>(session, selection, map);
}

template <typename Selection>
void delete_indicators(BIYOS_model &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Indicator, &Model::delete_indicators>(session, selection, map);
}

template <typename Selection>
void delete_special_ordered_sets(BIYOS_model &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::SpecialOrderedSet, &Model::delete_special_ordered_sets>(session,
                                                                                   selection,
                                                                                   map);
}

template <typename Selection>
void delete_cones(BIYOS_model &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Cone, &Model::delete_cones>(session, selection, map);
}

template <typename Selection>
void delete_complements(BIYOS_model &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Complement, &Model::delete_complements>(session, selection, map);
}

}

BIYOS_status BIYOS_delvarsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *map)
{
    return guarded<delete_variables<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delvarrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end, BIYOS_index *map)
{
    return guarded<delete_variables<Range>>(mdl, Range{beg, end}, map);
}

BIYOS_status BIYOS_delfunsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *map)
{
    return guarded<delete_functions<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delfunrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end, BIYOS_index *map)
{
    return guarded<delete_functions<Range>>(mdl, Range{beg, end}, map);
}

BIYOS_status BIYOS_delconsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *map)
{
    return guarded<delete_constraints<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delconrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end, BIYOS_index *map)
{
    return guarded<delete_constraints<Range>>(mdl, Range{beg, end}, map);
}

BIYOS_status BIYOS_delobjsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *map)
{
    return guarded<delete_objectives<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delobjrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end, BIYOS_index *map)
{
    return guarded<delete_objectives<Range>>(mdl, Range{beg, end}, map);
}

BIYOS_status BIYOS_delindsp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *map)
{
    return guarded<delete_indicators<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delindrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end, BIYOS_index *map)
{
    return guarded<delete_indicators<Range>>(mdl, Range{beg, end}, map);
}

BIYOS_status BIYOS_delsossp(BIYOS_model *mdl,
                            BIYOS_index n,
                            const BIYOS_index *idx,
                            BIYOS_index *map)
{
    return guarded<delete_special_ordered_sets<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delsosrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end, BIYOS_index *map)
{
    return guarded<delete_special_ordered_sets<Range>>(mdl, Range{beg, end}, map);
}

BIYOS_status BIYOS_delconesp(BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *idx,
                             BIYOS_index *map)
{
    return guarded<delete_cones<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delconerg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end, BIYOS_index *map)
{
    return guarded<delete_cones<Range>>(mdl, Range{beg, end}, map);
}

BIYOS_status BIYOS_delcconsp(BIYOS_model *mdl,
                             BIYOS_index n,
                             const BIYOS_index *idx,
                             BIYOS_index *map)
{
    return guarded<delete_complements<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delcconrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end, BIYOS_index *map)
{
    return guarded<delete_complements<Range>>(mdl, Range{beg, end}, map);
}
