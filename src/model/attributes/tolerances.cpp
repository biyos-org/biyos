#include "model/model.hpp"

#include "core/numbers.hpp"
#include "model/attributes/records.hpp"
#include "model/checks.hpp"

#include <cstddef>
#include <span>

namespace biyos
{

namespace
{

std::size_t slot(Family family) noexcept
{
    return static_cast<std::size_t>(family);
}

}

Tolerance Model::tolerance_of(Family family, Index index) const
{
    check_index(noun_of(family), index, n_of(family));
    return tolerances_[slot(family)].at(index);
}

void Model::set_tolerances(Family family,
                           std::span<const Index> indices,
                           std::span<const Real> absolute,
                           std::span<const Real> relative)
{
    tolerances_[slot(family)].set(noun_of(family), n_of(family), indices, absolute, relative);
}

void Model::delete_tolerances(Family family, std::span<const Index> indices)
{
    tolerances_[slot(family)].remove(noun_of(family), n_of(family), indices);
}

void Model::renumber_tolerances(Family family, std::span<const Index> map) noexcept
{
    tolerances_[slot(family)].renumber(map);
}

}
