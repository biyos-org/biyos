#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "model/checks.hpp"
#include "model/terms.hpp"

#include <algorithm>
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

[[noreturn]] void refuse_starts(const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, "argument 'starts'", reason);
}

void check_constant(Index function, double constant)
{
    const std::string_view fault = value_fault(constant);
    if (!fault.empty())
    {
        refuse(kNoun, function, "constant " + std::string(fault));
    }
}

template <typename T>
std::span<T> slice(std::span<T> terms, std::span<const Offset> starts, std::size_t k)
{
    const auto begin = static_cast<std::size_t>(starts[k]);
    const auto end = static_cast<std::size_t>(starts[k + 1]);
    return terms.subspan(begin, end - begin);
}

template <typename T> void reserve_more(std::vector<T> &items, std::size_t extra)
{
    const std::size_t wanted = items.size() + extra;
    if (wanted > items.capacity())
    {
        const std::size_t doubled = (std::max)(wanted, 2 * items.capacity());
        items.reserve((std::min)(doubled, items.size() + room(items)));
    }
}

}

void check_starts(std::span<const Offset> starts)
{
    if (!starts.empty() && starts.front() != 0)
    {
        refuse_starts("begins at " + text_of(starts.front()) + ", not 0");
    }
    for (std::size_t k = 1; k < starts.size(); ++k)
    {
        if (starts[k] < starts[k - 1])
        {
            const std::string fall = text_of(starts[k - 1]) + " to " + text_of(starts[k]);
            refuse_starts("decreases from " + fall + " at entry " + text_of(k));
        }
    }
}

void check_shape(std::size_t functions, std::span<const Offset> starts, std::size_t terms)
{
    if (starts.size() != functions + 1)
    {
        throw Error(ErrorKind::Internal, "model", "functions and starts differ in number");
    }
    check_starts(starts);
    if (starts.back() != static_cast<Offset>(terms))
    {
        throw Error(ErrorKind::Internal, "model", "starts and terms differ in number");
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
