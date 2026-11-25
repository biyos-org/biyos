#include "model/functions/merge.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "model/compressed.hpp"
#include "model/functions/pairs.hpp"
#include "model/functions/terms.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

std::pair<Index, Index> key_of(const QuadraticTerm &term) noexcept
{
    return pair_of(term);
}

Index key_of(const LinearTerm &term) noexcept
{
    return term.variable;
}

template <typename Term> bool before(const Term &a, const Term &b) noexcept
{
    return key_of(a) < key_of(b);
}

std::size_t start_of(std::span<const Offset> ends, std::size_t function)
{
    return function == 0 ? 0 : static_cast<std::size_t>(ends[function - 1]);
}

template <typename Term>
std::span<const Term> held_by(std::span<const Offset> ends,
                              std::span<const Term> terms,
                              std::size_t function)
{
    const std::size_t begin = start_of(ends, function);
    const auto end = static_cast<std::size_t>(ends[function]);
    return terms.subspan(begin, end - begin);
}

template <typename Term>
std::size_t new_terms(const Addition<Term> &addition, std::span<const Term> held)
{
    std::size_t found = 0;
    auto from = held.begin();
    for (const Term &term : addition.terms)
    {
        from = std::ranges::lower_bound(from, held.end(), term, before<Term>);
        if (from != held.end() && key_of(*from) == key_of(term))
        {
            check_sum(addition.function, *from, term);
            ++found;
        }
    }
    return addition.terms.size() - found;
}

template <typename Term> Term summed(Term held, const Term &given) noexcept
{
    held.coefficient += given.coefficient;
    return held;
}

template <typename Term>
void move_right(std::span<Term> terms, std::size_t begin, std::size_t end, std::size_t shift)
{
    if (shift == 0)
    {
        return;
    }
    std::ranges::copy_backward(terms.subspan(begin, end - begin), terms.first(end + shift).end());
}

template <typename Term>
void merge_back(std::span<Term> area, std::size_t held, std::span<const Term> given)
{
    std::size_t read = held;
    std::size_t write = area.size();
    std::size_t next = given.size();
    while (next > 0)
    {
        const Term &term = given[next - 1];
        --write;
        if (read == 0 || before(area[read - 1], term))
        {
            area[write] = term;
            --next;
        }
        else if (before(term, area[read - 1]))
        {
            --read;
            area[write] = area[read];
        }
        else
        {
            --read;
            --next;
            area[write] = summed(area[read], term);
        }
    }
    move_right(area, 0, read, write - read);
}

void shift_ends(std::span<Offset> ends, std::size_t from, std::size_t upto, std::size_t shift)
{
    if (shift == 0)
    {
        return;
    }
    for (std::size_t function = from; function < upto; ++function)
    {
        ends[function] += static_cast<Offset>(shift);
    }
}

template <typename Term> std::size_t added_by(std::span<const Insertion<Term>> insertions) noexcept
{
    std::size_t shift = 0;
    for (const Insertion<Term> &insertion : insertions)
    {
        shift += insertion.added;
    }
    return shift;
}

template <typename Term>
void insert(std::vector<Offset> &ends,
            std::vector<Term> &terms,
            std::span<const Insertion<Term>> insertions) noexcept
{
    std::size_t shift = added_by(insertions);
    std::size_t tail = terms.size();
    terms.resize(tail + shift);
    std::size_t upto = ends.size();
    for (const Insertion<Term> &insertion : insertions | std::views::reverse)
    {
        const std::size_t begin = start_of(ends, insertion.function);
        const auto end = static_cast<std::size_t>(ends[insertion.function]);
        move_right<Term>(terms, end, tail, shift);
        const std::span<Term> area = std::span(terms).subspan(begin, end - begin + shift);
        merge_back(area, end - begin, insertion.terms);
        shift_ends(ends, insertion.function, upto, shift);
        shift -= insertion.added;
        tail = begin;
        upto = insertion.function;
    }
}

template <typename Term> std::size_t function_of(const Insertion<Term> &insertion) noexcept
{
    return insertion.function;
}

}

template <typename Term>
Merge<Term>::Merge(std::vector<Offset> &ends,
                   std::vector<Term> &terms,
                   std::span<const Addition<Term>> additions)
    : ends_(ends), terms_(terms)
{
    insertions_.reserve(additions.size());
    for (const Addition<Term> &addition : additions)
    {
        const auto function = static_cast<std::size_t>(addition.function);
        const std::size_t added = new_terms(addition, held_by<Term>(ends, terms, function));
        insertions_.push_back(Insertion<Term>{
            .function = function,
            .terms = addition.terms,
            .added = added,
        });
    }
    std::ranges::sort(insertions_, {}, function_of<Term>);
    reserve_more(terms_, added_by<Term>(insertions_));
}

template <typename Term> void Merge<Term>::apply() noexcept
{
    insert<Term>(ends_, terms_, insertions_);
}

template class Merge<LinearTerm>;
template class Merge<QuadraticTerm>;

void merge_pairs(std::vector<Offset> &ends,
                 std::vector<QuadraticTerm> &terms,
                 std::span<const AddedPairs> additions)
{
    Merge(ends, terms, additions).apply();
}

void merge_terms(std::vector<Offset> &ends,
                 std::vector<LinearTerm> &terms,
                 std::span<const AddedTerms> additions)
{
    Merge(ends, terms, additions).apply();
}

}
