#pragma once

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"

#include <span>

namespace biyos
{

void check_terms(Index function, std::span<const LinearTerm> terms, Index variables);
void sort_terms(Index function, std::span<LinearTerm> terms);
void check_sum(Index function, const LinearTerm &held, const LinearTerm &given);

}
