#pragma once

#include "core/index.hpp"
#include "expr/terms.hpp"

#include <span>
#include <vector>

namespace biyos
{

struct AddedPairs
{
    Index function;
    std::span<const QuadraticTerm> terms;
};

void merge_pairs(std::vector<Offset> &ends,
                 std::vector<QuadraticTerm> &terms,
                 std::span<const AddedPairs> additions);

}
