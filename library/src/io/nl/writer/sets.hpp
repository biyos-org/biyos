#pragma once

#include "core/numbers.hpp"
#include "model/model.hpp"

#include <ostream>
#include <span>

namespace biyos::nl
{

void print_sets(std::ostream &output, const Model &model, std::span<const Index> position);

}
