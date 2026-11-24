#include "capi/model/selection.hpp"

#include "capi/arrays.hpp"
#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text/text.hpp"
#include "model/checks.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::capi
{

std::span<const Index> selected(const Model &, Family, List list)
{
    return listed(list.n, list.indices);
}

Index length_of(const Model &model, Family family, Range range)
{
    const std::string_view noun = noun_of(family);
    const Index total = model.n_of(family);
    if (range.start < 0 || range.start > total)
    {
        refuse(noun, range.start, "unknown");
    }
    if (range.stop < range.start)
    {
        const std::string reason = text_of(range.stop) + " is below 'beg' " + text_of(range.start);
        throw Error(ErrorKind::InvalidArgument, "argument 'end'", reason);
    }
    if (range.stop > total)
    {
        refuse(noun, total, "unknown");
    }
    return range.stop - range.start;
}

std::vector<Index> selected(const Model &model, Family family, Range range)
{
    std::vector<Index> indices(static_cast<std::size_t>(length_of(model, family, range)));
    std::iota(indices.begin(), indices.end(), range.start);
    return indices;
}

}
