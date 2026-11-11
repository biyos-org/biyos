#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"
#include "model/compressed.hpp"
#include "model/terms.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
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

}

Index Model::function_count() const noexcept
{
    return static_cast<Index>(constants_.size());
}

std::span<const double> Model::constants(Index first, Index count) const
{
    check_span(kNoun, first, count, function_count());
    const auto start = static_cast<std::size_t>(first);
    return std::span(constants_).subspan(start, static_cast<std::size_t>(count));
}

std::span<const LinearTerm> Model::terms(Index first, Index count) const
{
    check_span(kNoun, first, count, function_count());
    const auto begin = static_cast<std::size_t>(begin_of(static_cast<std::size_t>(first)));
    const auto end = static_cast<std::size_t>(begin_of(static_cast<std::size_t>(first + count)));
    return std::span(terms_).subspan(begin, end - begin);
}

void Model::check_room(std::size_t functions, Offset terms) const
{
    check_count(kNoun, constants_, functions);
    if (terms > static_cast<Offset>(room(terms_)))
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
        check_terms(function, part, variable_count());
        sort_terms(function, part);
    }
    append(constants, starts, terms);
}

Offset Model::begin_of(std::size_t function) const noexcept
{
    return function == 0 ? 0 : ends_[function - 1];
}

void Model::append(std::span<const double> constants,
                   std::span<const Offset> starts,
                   std::span<const LinearTerm> terms)
{
    reserve_more(constants_, constants.size());
    reserve_more(ends_, constants.size());
    reserve_more(terms_, terms.size());
    const auto base = static_cast<Offset>(terms_.size());
    constants_.insert(constants_.end(), constants.begin(), constants.end());
    for (const Offset end : starts.subspan(1))
    {
        ends_.push_back(base + end);
    }
    terms_.insert(terms_.end(), terms.begin(), terms.end());
}

}
