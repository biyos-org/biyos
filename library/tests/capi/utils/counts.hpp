#pragma once

#include "biyos/biyos.h"

#include <cstddef>
#include <vector>

namespace biyos::test
{

inline std::vector<BIYOS_index> expanded(const std::vector<BIYOS_offset> &counts,
                                         const std::vector<BIYOS_index> &listed)
{
    std::vector<BIYOS_index> records;
    for (std::size_t j = 0; j < counts.size(); ++j)
    {
        records.insert(records.end(), static_cast<std::size_t>(counts[j]), listed[j]);
    }
    return records;
}

}
