#pragma once

#include "core/index.hpp"
#include "expr/terms.hpp"

#include <span>

namespace biyos
{

void check_terms(Index fun, std::span<const LinearTerm> terms, Index vars);
void sort_terms(Index fun, std::span<LinearTerm> terms);

}
