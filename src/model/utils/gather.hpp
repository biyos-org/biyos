#pragma once

#include "core/numbers.hpp"
#include "model/model.hpp"

#include <span>
#include <vector>

namespace biyos
{

template <typename Record>
std::vector<Record> picked(const Model &model,
                           const Record &(Model::*read)(Index) const,
                           std::span<const Index> indices)
{
    std::vector<Record> found;
    found.reserve(indices.size());
    for (const Index i : indices)
    {
        found.push_back((model.*read)(i));
    }
    return found;
}

}
