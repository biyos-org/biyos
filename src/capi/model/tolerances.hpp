#pragma once

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/attributes/records.hpp"
#include "model/model.hpp"
#include "model/utils/gather.hpp"

#include <span>
#include <vector>

namespace biyos::capi
{

template <typename Selection>
void set_tolerances(Session &session,
                    Family family,
                    Selection selection,
                    const BIYOS_real *absolute,
                    const BIYOS_real *relative)
{
    Model &model = writable(session);
    const auto changed = selected(model, family, selection);
    model.set_tolerances(family,
                         changed,
                         given(absolute, changed.size()),
                         given(relative, changed.size()));
}

template <typename Selection>
void get_tolerances(const Session &session,
                    Family family,
                    Selection selection,
                    BIYOS_real *absolute,
                    BIYOS_real *relative)
{
    const Model &model = session.model();
    const auto chosen = selected(model, family, selection);
    const std::vector<Tolerance> tolerances =
        picked(chosen, [&](Index index) { return model.tolerance_of(family, index); });
    write_field(tolerances, &Tolerance::absolute, absolute);
    write_field(tolerances, &Tolerance::relative, relative);
}

template <typename Selection>
void delete_tolerances(Session &session, Family family, Selection selection)
{
    Model &model = writable(session);
    model.delete_tolerances(family, selected(model, family, selection));
}

}
