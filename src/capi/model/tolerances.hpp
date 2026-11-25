#pragma once

#include "capi/model/selection.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "model/attributes/overrides.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos::capi
{

template <typename Selection>
void set_tolerances(BIYOS_model &model,
                    Family family,
                    Selection selection,
                    const double *absolute,
                    const double *relative)
{
    Model &core = writable(model);
    const auto changed = selected(core, family, selection);
    core.set_tolerances(family,
                        changed,
                        given(absolute, changed.size()),
                        given(relative, changed.size()));
}

template <typename Selection>
void get_tolerances(const BIYOS_model &model,
                    Family family,
                    Selection selection,
                    double *absolute,
                    double *relative)
{
    const auto chosen = selected(model.core, family, selection);
    std::vector<Override> tolerances;
    tolerances.reserve(chosen.size());
    for (const Index index : chosen)
    {
        tolerances.push_back(model.core.tolerance_of(family, index));
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
void delete_tolerances(BIYOS_model &model, Family family, Selection selection)
{
    Model &core = writable(model);
    core.delete_tolerances(family, selected(core, family, selection));
}

}
