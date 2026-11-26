#include "biyos/biyos.h"

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/timing/timer.hpp"
#include "model/model.hpp"

#include <span>
#include <vector>

using biyos::EntryTimer;
using biyos::Family;
using biyos::Index;
using biyos::kModule;
using biyos::Model;
using biyos::Session;
using biyos::capi::guarded;
using biyos::capi::List;
using biyos::capi::Range;
using biyos::capi::selected;
using biyos::capi::writable;
using biyos::capi::write_array;

namespace
{

template <auto Delete>
std::vector<Index> delete_chosen(Session &session, Model &model, std::span<const Index> chosen)
{
    const EntryTimer timer(session.stream(), kModule, "delete");
    return (model.*Delete)(chosen);
}

template <Family Target, auto Delete, typename Selection>
void delete_records(Session &session, Selection selection, BIYOS_index *map)
{
    Model &model = writable(session);
    write_array(delete_chosen<Delete>(session, model, selected(model, Target, selection)), map);
}

template <typename Selection>
void delete_variables(Session &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Variable, &Model::delete_variables>(session, selection, map);
}

template <typename Selection>
void delete_functions(Session &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Function, &Model::delete_functions>(session, selection, map);
}

template <typename Selection>
void delete_constraints(Session &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Constraint, &Model::delete_constraints>(session, selection, map);
}

template <typename Selection>
void delete_objectives(Session &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Objective, &Model::delete_objectives>(session, selection, map);
}

template <typename Selection>
void delete_indicators(Session &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Indicator, &Model::delete_indicators>(session, selection, map);
}

template <typename Selection>
void delete_special_ordered_sets(Session &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::SpecialOrderedSet, &Model::delete_special_ordered_sets>(session,
                                                                                   selection,
                                                                                   map);
}

template <typename Selection>
void delete_cones(Session &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Cone, &Model::delete_cones>(session, selection, map);
}

template <typename Selection>
void delete_complements(Session &session, Selection selection, BIYOS_index *map)
{
    delete_records<Family::Complement, &Model::delete_complements>(session, selection, map);
}

template <typename Selection>
void delete_callbacks(Session &session, Selection selection, BIYOS_index *map)
{
    Model &model = writable(session);
    const auto chosen = selected("callback", model.n_callbacks(), selection);
    write_array(delete_chosen<&Model::delete_callbacks>(session, model, chosen), map);
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

BIYOS_status BIYOS_delcbsp(BIYOS_model *mdl,
                           BIYOS_index n,
                           const BIYOS_index *idx,
                           BIYOS_index *map)
{
    return guarded<delete_callbacks<List>>(mdl, List{n, idx}, map);
}

BIYOS_status BIYOS_delcbrg(BIYOS_model *mdl, BIYOS_index beg, BIYOS_index end, BIYOS_index *map)
{
    return guarded<delete_callbacks<Range>>(mdl, Range{beg, end}, map);
}
