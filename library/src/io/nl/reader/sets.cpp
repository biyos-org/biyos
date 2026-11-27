#include "io/nl/reader/sets.hpp"

#include "core/numbers.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace biyos::nl
{

namespace
{

struct Entry
{
    Offset number;
    Index variable;
};

std::pair<Offset, bool> rank_of(const Entry &entry) noexcept
{
    const Offset number = entry.number;
    return {number < 0 ? -(number + 1) : number - 1, number < 0};
}

bool weighed(std::span<const Entry> set, std::span<const std::optional<Real>> weights) noexcept
{
    for (const Entry &entry : set)
    {
        if (weights[to_size(entry.variable)])
        {
            return true;
        }
    }
    return false;
}

void add_set(Sets &sets, std::span<const Entry> set, std::span<const std::optional<Real>> weights)
{
    const bool given = weighed(set, weights);
    sets.kinds.push_back(set.front().number > 0 ? SpecialOrderedSetKind::One
                                                : SpecialOrderedSetKind::Two);
    sets.starts.push_back(to_offset(sets.members.size()));
    for (std::size_t k = 0; k < set.size(); ++k)
    {
        const Real missing = given ? 0.0 : static_cast<Real>(k + 1);
        const std::optional<Real> &weight = weights[to_size(set[k].variable)];
        sets.members.push_back({.variable = set[k].variable, .weight = weight.value_or(missing)});
    }
}

}

Sets sets_of(std::span<const Offset> numbers, std::span<const std::optional<Real>> weights)
{
    std::vector<Entry> entries;
    for (std::size_t k = 0; k < numbers.size(); ++k)
    {
        if (numbers[k] != 0)
        {
            entries.push_back({.number = numbers[k], .variable = to_index(k)});
        }
    }
    std::ranges::stable_sort(entries, {}, rank_of);
    Sets sets{.kinds = {}, .starts = {}, .members = {}};
    std::size_t begin = 0;
    for (std::size_t k = 1; k <= entries.size(); ++k)
    {
        if (k == entries.size() || entries[k].number != entries[begin].number)
        {
            add_set(sets, std::span(entries).subspan(begin, k - begin), weights);
            begin = k;
        }
    }
    sets.starts.push_back(to_offset(sets.members.size()));
    return sets;
}

}
