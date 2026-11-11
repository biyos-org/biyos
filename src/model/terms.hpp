#pragma once

#include "core/index.hpp"
#include "expr/terms.hpp"

#include <span>

namespace biyos
{

void check_terms(Index function, std::span<const LinearTerm> terms, Index variables);
void sort_terms(Index function, std::span<LinearTerm> terms);

}
