#pragma once

#include "core/numbers.hpp"
#include "model/model.hpp"

#include <optional>
#include <span>
#include <vector>

namespace biyos::nl
{

struct Sets
{
    std::vector<SpecialOrderedSetKind> kinds;
    std::vector<Offset> starts;
    std::vector<Member> members;
};

Sets sets_of(std::span<const Offset> numbers, std::span<const std::optional<Real>> weights);

}
