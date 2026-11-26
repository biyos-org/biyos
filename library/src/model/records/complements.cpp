#include "model/model.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "model/utils/checks.hpp"
#include "model/utils/renumber.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "complement";

void check_bounds(Index index, const Complement &complement)
{
    const std::string_view fault = bound_fault(complement.lower, complement.upper);
    if (!fault.empty())
    {
        refuse(kNoun, index, std::string(fault));
    }
}

void check(Index index, const Complement &complement, Index functions)
{
    check_function(kNoun, index, complement.function, functions);
    check_function(kNoun, index, complement.multiplier, functions);
    check_bounds(index, complement);
}

}

Index Model::n_complements() const noexcept
{
    return to_index(complements_.size());
}

const Complement &Model::complement(Index index) const
{
    check_index(kNoun, index, n_complements());
    return complements_[to_size(index)];
}

std::span<const Complement> Model::complements(Index start, Index stop) const
{
    check_span(kNoun, start, stop, n_complements());
    const auto offset = to_size(start);
    return std::span(complements_).subspan(offset, to_size(stop - start));
}

void Model::add_complements(std::span<const Complement> added)
{
    check_n(kNoun, complements_, added.size());
    for (std::size_t k = 0; k < added.size(); ++k)
    {
        check(to_index(complements_.size() + k), added[k], n_functions());
    }
    complements_.insert(complements_.end(), added.begin(), added.end());
}

void Model::set_complements(std::span<const Index> indices, std::span<const Complement> changed)
{
    if (indices.size() != changed.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and complements differ in number");
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        check_index(kNoun, indices[k], n_complements());
        check(indices[k], changed[k], n_functions());
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        complements_[to_size(indices[k])] = changed[k];
    }
}

std::vector<Index> Model::delete_complements(std::span<const Index> indices)
{
    std::vector<Index> map = renumbering(kNoun, n_complements(), indices);
    keep(complements_, map);
    renumber_tolerances(Family::Complement, map);
    renumber_names(Family::Complement, map);
    return map;
}

}
