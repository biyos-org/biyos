#include "model/model.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "model/checks.hpp"
#include "model/compressed.hpp"
#include "model/merge.hpp"
#include "model/pairs.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kNoun = "function";

}

std::span<const QuadraticTerm> Model::quad_terms(Index beg, Index n) const
{
    check_span(kNoun, beg, n, n_funs());
    const auto begin = static_cast<std::size_t>(quad_begin(static_cast<std::size_t>(beg)));
    const auto last = static_cast<std::size_t>(beg + n);
    const auto end = static_cast<std::size_t>(quad_begin(last));
    return std::span(quadratic_terms_).subspan(begin, end - begin);
}

void Model::check_quad_room(Offset terms) const
{
    if (terms > static_cast<Offset>(room(quadratic_terms_)))
    {
        throw Error(ErrorKind::NoMemory, "model", "no quadratic term offset left");
    }
}

void Model::add_quad_terms(std::span<const Index> funs,
                           std::span<const Offset> starts,
                           std::vector<QuadraticTerm> terms)
{
    check_quad_room(static_cast<Offset>(terms.size()));
    check_shape(funs.size(), starts, terms.size());
    check_funs(funs, n_funs());
    std::vector<AddedPairs> additions;
    additions.reserve(funs.size());
    for (std::size_t k = 0; k < funs.size(); ++k)
    {
        const std::span<QuadraticTerm> part = slice(std::span(terms), starts, k);
        check_pairs(funs[k], part, n_vars());
        sort_pairs(funs[k], part);
        additions.push_back(AddedPairs{.function = funs[k], .terms = part});
    }
    merge_pairs(quadratic_ends_, quadratic_terms_, additions);
}

Offset Model::quad_begin(std::size_t fun) const noexcept
{
    return fun == 0 ? 0 : quadratic_ends_[fun - 1];
}

}
