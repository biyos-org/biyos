#include "capi/model/selection.hpp"

#include "capi/utils/arrays.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"

#include <cstddef>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

namespace biyos::capi
{

std::span<const Index> selected(Family family, Index total, List list)
{
    return selected(noun_of(family), total, list);
}

std::vector<Index> selected(Family family, Index total, Range range)
{
    return selected(noun_of(family), total, range);
}

std::span<const Index> selected(std::string_view, Index, List list)
{
    return listed(list.n, list.indices);
}

std::vector<Index> selected(std::string_view noun, Index total, Range range)
{
    check_span(noun, range.start, range.stop, total);
    std::vector<Index> indices(to_size(range.stop - range.start));
    std::iota(indices.begin(), indices.end(), range.start);
    return indices;
}

void check_known(Family family, Index total, std::span<const Index> indices)
{
    for (const Index index : indices)
    {
        check_index(noun_of(family), index, total);
    }
}

}
