#pragma once

#include "io/nl/source/bounds.hpp"
#include "io/nl/source/source.hpp"
#include "model/model.hpp"

#include <span>

namespace biyos::nl
{

void add_complements(const Source &source,
                     Model &model,
                     Index first,
                     std::span<const Pairing> pairings);

}
