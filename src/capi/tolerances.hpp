#pragma once

#include "capi/arrays.hpp"
#include "capi/entry.hpp"
#include "capi/selection.hpp"
#include "core/index.hpp"
#include "model/model.hpp"
#include "model/overrides.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos::capi
{

struct ToleranceCalls
{
    Family family;
    Override (Model::*read)(Index) const;
    void (Model::*set)(std::span<const Index>, std::span<const double>, std::span<const double>);
    void (Model::*remove)(std::span<const Index>);
};

template <typename Selection>
void set_tolerances(BIYOS_model &model,
                    ToleranceCalls calls,
                    Selection selection,
                    const double *absolute,
                    const double *relative)
{
    Model &core = writable(model);
    const auto changed = selected(core, calls.family, selection);
    (core.*calls.set)(changed, given(absolute, changed.size()), given(relative, changed.size()));
}

template <typename Selection>
void get_tolerances(const BIYOS_model &model,
                    ToleranceCalls calls,
                    Selection selection,
                    double *absolute,
                    double *relative)
{
    const auto chosen = selected(model.core, calls.family, selection);
    std::vector<Override> tolerances;
    tolerances.reserve(chosen.size());
    for (const Index index : chosen)
    {
        tolerances.push_back((model.core.*calls.read)(index));
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
void delete_tolerances(BIYOS_model &model, ToleranceCalls calls, Selection selection)
{
    Model &core = writable(model);
    (core.*calls.remove)(selected(core, calls.family, selection));
}

}
