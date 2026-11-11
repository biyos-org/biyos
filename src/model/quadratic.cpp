#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"
#include "model/compressed.hpp"
#include "model/merge.hpp"
#include "model/pairs.hpp"

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

void check_functions(std::span<const Index> functions, Index held)
{
    for (const Index function : functions)
    {
        check_index(kNoun, function, held);
    }
    std::vector<Index> sorted(functions.begin(), functions.end());
    std::ranges::sort(sorted);
    const auto repeat = std::ranges::adjacent_find(sorted);
    if (repeat != sorted.end())
    {
        const std::string reason = item(kNoun, *repeat) + " is repeated";
        throw Error(ErrorKind::InvalidArgument, "argument 'functions'", reason);
    }
}

}

std::span<const QuadraticTerm> Model::quadratic_terms(Index first, Index count) const
{
    check_span(kNoun, first, count, function_count());
    const auto begin = static_cast<std::size_t>(quadratic_begin(static_cast<std::size_t>(first)));
    const auto last = static_cast<std::size_t>(first + count);
    const auto end = static_cast<std::size_t>(quadratic_begin(last));
    return std::span(quadratic_terms_).subspan(begin, end - begin);
}

void Model::check_quadratic_room(Offset terms) const
{
    if (terms > static_cast<Offset>(room(quadratic_terms_)))
    {
        throw Error(ErrorKind::NoMemory, "model", "no quadratic term offset left");
    }
}

void Model::add_quadratic_terms(std::span<const Index> functions,
                                std::span<const Offset> starts,
                                std::vector<QuadraticTerm> terms)
{
    check_quadratic_room(static_cast<Offset>(terms.size()));
    check_shape(functions.size(), starts, terms.size());
    check_functions(functions, function_count());
    std::vector<AddedPairs> additions;
    additions.reserve(functions.size());
    for (std::size_t k = 0; k < functions.size(); ++k)
    {
        const std::span<QuadraticTerm> part = slice(std::span(terms), starts, k);
        check_pairs(functions[k], part, variable_count());
        sort_pairs(functions[k], part);
        additions.push_back(AddedPairs{.function = functions[k], .terms = part});
    }
    merge_pairs(quadratic_ends_, quadratic_terms_, additions);
}

Offset Model::quadratic_begin(std::size_t function) const noexcept
{
    return function == 0 ? 0 : quadratic_ends_[function - 1];
}

}
