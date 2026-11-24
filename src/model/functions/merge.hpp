#pragma once

#include "core/index.hpp"
#include "expr/forms/terms.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

template <typename Term> struct Addition
{
    Index function;
    std::span<const Term> terms;
};

template <typename Term> struct Insertion
{
    std::size_t function;
    std::span<const Term> terms;
    std::size_t added;
};

template <typename Term> class Merge
{
  public:
    Merge(std::vector<Offset> &ends,
          std::vector<Term> &terms,
          std::span<const Addition<Term>> additions);

    void apply() noexcept;

  private:
    std::vector<Offset> &ends_;
    std::vector<Term> &terms_;
    std::vector<Insertion<Term>> insertions_;
};

extern template class Merge<LinearTerm>;
extern template class Merge<QuadraticTerm>;

using AddedPairs = Addition<QuadraticTerm>;
using AddedTerms = Addition<LinearTerm>;

void merge_pairs(std::vector<Offset> &ends,
                 std::vector<QuadraticTerm> &terms,
                 std::span<const AddedPairs> additions);
void merge_terms(std::vector<Offset> &ends,
                 std::vector<LinearTerm> &terms,
                 std::span<const AddedTerms> additions);

}
