#pragma once

#include "core/index.hpp"
#include "expr/terms.hpp"

#include <span>
#include <vector>

namespace biyos
{

template <typename Term> struct Addition
{
    Index function;
    std::span<const Term> terms;
};

using AddedPairs = Addition<QuadraticTerm>;
using AddedTerms = Addition<LinearTerm>;

void merge_pairs(std::vector<Offset> &ends,
                 std::vector<QuadraticTerm> &terms,
                 std::span<const AddedPairs> additions);
void merge_terms(std::vector<Offset> &ends,
                 std::vector<LinearTerm> &terms,
                 std::span<const AddedTerms> additions);

}
