#pragma once

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/attributes/records.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos::capi
{

template <typename Selection>
void set_tolerances(BIYOS_model &session,
                    Family family,
                    Selection selection,
                    const BIYOS_real *absolute,
                    const BIYOS_real *relative)
{
    Model &core = writable(session);
    const auto changed = selected(core, family, selection);
    core.set_tolerances(family,
                        changed,
                        given(absolute, changed.size()),
                        given(relative, changed.size()));
}

template <typename Selection>
void get_tolerances(const BIYOS_model &session,
                    Family family,
                    Selection selection,
                    BIYOS_real *absolute,
                    BIYOS_real *relative)
{
    const auto chosen = selected(session.model(), family, selection);
    std::vector<Tolerance> tolerances;
    tolerances.reserve(chosen.size());
    for (const Index index : chosen)
    {
        tolerances.push_back(session.model().tolerance_of(family, index));
    }
    for (std::size_t k = 0; k < tolerances.size(); ++k)
    {
        if (absolute != nullptr)
        {
            absolute[k] = tolerances[k].absolute;
        }
        if (relative != nullptr)
        {
            relative[k] = tolerances[k].relative;
        }
    }
}

template <typename Selection>
void delete_tolerances(BIYOS_model &session, Family family, Selection selection)
{
    Model &core = writable(session);
    core.delete_tolerances(family, selected(core, family, selection));
}

}
