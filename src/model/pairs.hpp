#pragma once

#include "core/index.hpp"
#include "expr/terms.hpp"

#include <span>
#include <string>
#include <utility>

namespace biyos
{

inline std::pair<Index, Index> pair_of(const QuadraticTerm &term) noexcept
{
    return {term.first, term.second};
}

std::string pair_name(const QuadraticTerm &term);
void check_pairs(Index function, std::span<const QuadraticTerm> terms, Index variables);
void sort_pairs(Index function, std::span<QuadraticTerm> terms);
void check_sum(Index function, const QuadraticTerm &held, const QuadraticTerm &given);

}
