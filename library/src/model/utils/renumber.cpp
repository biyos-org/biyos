#include "model/utils/renumber.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

std::vector<Index> renumbering(std::string_view noun,
                               std::string_view word,
                               Index total,
                               std::span<const Index> deleted)
{
    for (const Index index : deleted)
    {
        check_index(noun, index, total);
    }
    std::vector<Index> map(to_size(total), 0);
    for (const Index index : deleted)
    {
        Index &entry = map[to_size(index)];
        if (entry == kDeleted)
        {
            throw Error(ErrorKind::InvalidArgument,
                        "argument '" + std::string(word) + "'",
                        item(noun, index) + " is repeated");
        }
        entry = kDeleted;
    }
    Index next = 0;
    for (Index &entry : map)
    {
        if (entry != kDeleted)
        {
            entry = next;
            ++next;
        }
    }
    return map;
}

std::vector<Index> renumbering(Family family, Index total, std::span<const Index> deleted)
{
    return renumbering(noun_of(family), word_of(family), total, deleted);
}

void renumber(Index &index, std::span<const Index> map) noexcept
{
    index = map[to_size(index)];
}

void renumber_all(std::span<Index> indices, std::span<const Index> map) noexcept
{
    for (Index &index : indices)
    {
        renumber(index, map);
    }
}

}
