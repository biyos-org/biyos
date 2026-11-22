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
    return {term.row, term.column};
}

std::string pair_name(const QuadraticTerm &term);
void check_pairs(Index fun, std::span<const QuadraticTerm> terms, Index vars);
void sort_pairs(Index fun, std::span<QuadraticTerm> terms);
void check_sum(Index fun, const QuadraticTerm &held, const QuadraticTerm &given);

}
