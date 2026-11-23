#include "model/renumber.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace biyos
{

std::vector<Index> renumbering(std::string_view noun, Index total, std::span<const Index> deleted)
{
    for (const Index index : deleted)
    {
        check_index(noun, index, total);
    }
    std::vector<Index> map(static_cast<std::size_t>(total), 0);
    for (const Index index : deleted)
    {
        Index &entry = map[static_cast<std::size_t>(index)];
        if (entry == kDeleted)
        {
            throw Error(ErrorKind::InvalidArgument,
                        "argument 'idx'",
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

}
