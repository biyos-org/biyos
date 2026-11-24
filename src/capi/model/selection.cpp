#include "capi/model/selection.hpp"

#include "capi/arrays.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <numeric>
#include <span>
#include <vector>

namespace biyos::capi
{

std::span<const Index> selected(const Model &, Family, List list)
{
    return listed(list.n, list.indices);
}

std::vector<Index> selected(const Model &model, Family family, Range range)
{
    check_span(noun_of(family), range.start, range.size, model.n_of(family));
    std::vector<Index> indices(static_cast<std::size_t>(range.size));
    std::iota(indices.begin(), indices.end(), range.start);
    return indices;
}

}
