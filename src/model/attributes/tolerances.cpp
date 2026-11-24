#include "model/model.hpp"

#include "core/index.hpp"
#include "model/attributes/overrides.hpp"
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

Override Model::tolerance_of(Family family, Index index) const
{
    check_index(noun_of(family), index, n_of(family));
    return tolerances_[slot(family)].at(index);
}

void Model::set_tolerances(Family family,
                           std::span<const Index> indices,
                           std::span<const double> absolute,
                           std::span<const double> relative)
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
