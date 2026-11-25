#include "capi/model/selection.hpp"

#include "capi/utils/arrays.hpp"
#include "core/numbers.hpp"
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
    check_span(noun_of(family), range.start, range.stop, model.n_of(family));
    std::vector<Index> indices(static_cast<std::size_t>(range.stop - range.start));
    std::iota(indices.begin(), indices.end(), range.start);
    return indices;
}

}
