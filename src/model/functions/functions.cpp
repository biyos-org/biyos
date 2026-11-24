#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"
#include "model/compressed.hpp"
#include "model/functions/merge.hpp"
#include "model/functions/terms.hpp"
#include "model/renumber.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "function";

void check_constant(Index function, double constant)
{
    const std::string_view fault = value_fault(constant);
    if (!fault.empty())
    {
        refuse(kNoun, function, "constant " + std::string(fault));
    }
}

void check_constants(std::span<const Index> indices,
                     std::span<const double> values,
                     Index functions)
{
    if (indices.size() != values.size())
    {
        throw Error(ErrorKind::Internal, "model", "indices and constants differ in number");
    }
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        check_index(kNoun, indices[k], functions);
        check_constant(indices[k], values[k]);
    }
}

void check_sum(Index function, double sum)
{
    const std::string_view fault = value_fault(sum);
    if (!fault.empty())
    {
        refuse(kNoun, function, "constant sum " + std::string(fault));
    }
}

}

Index Model::n_functions() const noexcept
{
    return static_cast<Index>(constants_.size());
}

std::span<const double> Model::constants(Index start, Index size) const
{
    check_span(kNoun, start, size, n_functions());
    const auto offset = static_cast<std::size_t>(start);
    return std::span(constants_).subspan(offset, static_cast<std::size_t>(size));
}

const double &Model::constant(Index index) const
{
    check_index(kNoun, index, n_functions());
    return constants_[static_cast<std::size_t>(index)];
}

void Model::set_constants(std::span<const Index> indices, std::span<const double> values)
{
    check_constants(indices, values, n_functions());
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        constants_[static_cast<std::size_t>(indices[k])] = values[k];
    }
}

void Model::add_constants(std::span<const Index> indices, std::span<const double> values)
{
    check_constants(indices, values, n_functions());
    std::unordered_map<Index, double> sums;
    for (std::size_t k = 0; k < indices.size(); ++k)
    {
        const auto held = sums.try_emplace(indices[k], constant(indices[k])).first;
        held->second += values[k];
        check_sum(indices[k], held->second);
    }
    for (const auto &[function, sum] : sums)
    {
        constants_[static_cast<std::size_t>(function)] = sum;
    }
}

std::span<const LinearTerm> Model::linear_terms(Index start, Index size) const
{
    check_span(kNoun, start, size, n_functions());
    const auto begin = static_cast<std::size_t>(begin_of(static_cast<std::size_t>(start)));
    const auto end = static_cast<std::size_t>(begin_of(static_cast<std::size_t>(start + size)));
    return std::span(linear_terms_).subspan(begin, end - begin);
}

void Model::check_room(std::size_t functions, Offset terms) const
{
    check_n(kNoun, constants_, functions);
    if (terms > static_cast<Offset>(room(linear_terms_)))
    {
        throw Error(ErrorKind::NoMemory, "model", "no term offset left");
    }
}

void Model::add_functions(std::span<const double> constants,
                          std::span<const Offset> starts,
                          std::vector<LinearTerm> terms)
{
    check_room(constants.size(), static_cast<Offset>(terms.size()));
    check_shape(constants.size(), starts, terms.size());
    for (std::size_t k = 0; k < constants.size(); ++k)
    {
        const auto function = static_cast<Index>(constants_.size() + k);
        const std::span<LinearTerm> part = slice(std::span(terms), starts, k);
        check_constant(function, constants[k]);
        check_terms(function, part, n_variables());
        sort_terms(function, part);
    }
    append(constants, starts, terms);
}

void Model::add_linear_terms(std::span<const Index> functions,
                             std::span<const Offset> starts,
                             std::vector<LinearTerm> terms)
{
    check_room(0, static_cast<Offset>(terms.size()));
    check_shape(functions.size(), starts, terms.size());
    check_functions(functions, n_functions(), "funidx");
    std::vector<AddedTerms> additions;
    additions.reserve(functions.size());
    for (std::size_t k = 0; k < functions.size(); ++k)
    {
        const std::span<LinearTerm> part = slice(std::span(terms), starts, k);
        check_terms(functions[k], part, n_variables());
        sort_terms(functions[k], part);
        additions.push_back(AddedTerms{.function = functions[k], .terms = part});
    }
    merge_terms(linear_ends_, linear_terms_, additions);
}

std::vector<Index> Model::delete_functions(std::span<const Index> indices)
{
    std::vector<Index> map = renumbering(kNoun, n_functions(), indices);
    check_unused(map);
    keep(constants_, map);
    keep_parts(linear_ends_, linear_terms_, map);
    keep_parts(quadratic_ends_, quadratic_terms_, map);
    keep(roots_, map);
    renumber_functions(map);
    renumber_names(Family::Function, map);
    return map;
}

Offset Model::begin_of(std::size_t function) const noexcept
{
    return function == 0 ? 0 : linear_ends_[function - 1];
}

void Model::append(std::span<const double> constants,
                   std::span<const Offset> starts,
                   std::span<const LinearTerm> terms)
{
    reserve_more(constants_, constants.size());
    reserve_more(linear_ends_, constants.size());
    reserve_more(linear_terms_, terms.size());
    reserve_more(quadratic_ends_, constants.size());
    reserve_more(roots_, constants.size());
    const auto base = static_cast<Offset>(linear_terms_.size());
    constants_.insert(constants_.end(), constants.begin(), constants.end());
    for (const Offset end : starts.subspan(1))
    {
        linear_ends_.push_back(base + end);
    }
    linear_terms_.insert(linear_terms_.end(), terms.begin(), terms.end());
    const auto held = static_cast<Offset>(quadratic_terms_.size());
    quadratic_ends_.insert(quadratic_ends_.end(), constants.size(), held);
    roots_.insert(roots_.end(), constants.size(), kNoRoot);
}

}
