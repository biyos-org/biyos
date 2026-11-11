#include "model/merge.hpp"

#include "core/index.hpp"
#include "expr/terms.hpp"
#include "model/compressed.hpp"
#include "model/pairs.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

struct Insertion
{
    std::size_t function;
    std::span<const QuadraticTerm> terms;
    std::size_t added;
};

std::size_t start_of(std::span<const Offset> ends, std::size_t function)
{
    return function == 0 ? 0 : static_cast<std::size_t>(ends[function - 1]);
}

std::span<const QuadraticTerm> held_by(std::span<const Offset> ends,
                                       std::span<const QuadraticTerm> terms,
                                       std::size_t function)
{
    const std::size_t begin = start_of(ends, function);
    const auto end = static_cast<std::size_t>(ends[function]);
    return terms.subspan(begin, end - begin);
}

std::size_t new_pairs(const AddedPairs &addition, std::span<const QuadraticTerm> held)
{
    std::size_t found = 0;
    auto from = held.begin();
    for (const QuadraticTerm &term : addition.terms)
    {
        from = std::ranges::lower_bound(from, held.end(), pair_of(term), {}, pair_of);
        if (from != held.end() && pair_of(*from) == pair_of(term))
        {
            check_sum(addition.function, *from, term);
            ++found;
        }
    }
    return addition.terms.size() - found;
}

QuadraticTerm summed(QuadraticTerm held, const QuadraticTerm &given) noexcept
{
    held.coefficient += given.coefficient;
    return held;
}

void move_right(std::span<QuadraticTerm> terms,
                std::size_t begin,
                std::size_t end,
                std::size_t shift)
{
    if (shift == 0)
    {
        return;
    }
    std::ranges::copy_backward(terms.subspan(begin, end - begin), terms.first(end + shift).end());
}

void merge_back(std::span<QuadraticTerm> area,
                std::size_t held,
                std::span<const QuadraticTerm> given)
{
    std::size_t read = held;
    std::size_t write = area.size();
    std::size_t next = given.size();
    while (next > 0)
    {
        const QuadraticTerm &term = given[next - 1];
        --write;
        if (read == 0 || pair_of(area[read - 1]) < pair_of(term))
        {
            area[write] = term;
            --next;
        }
        else if (pair_of(term) < pair_of(area[read - 1]))
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

void insert(std::vector<Offset> &ends,
            std::vector<QuadraticTerm> &terms,
            std::span<const Insertion> insertions)
{
    std::size_t shift = 0;
    for (const Insertion &insertion : insertions)
    {
        shift += insertion.added;
    }
    reserve_more(terms, shift);
    std::size_t tail = terms.size();
    terms.resize(tail + shift);
    std::size_t upto = ends.size();
    for (const Insertion &insertion : insertions | std::views::reverse)
    {
        const std::size_t begin = start_of(ends, insertion.function);
        const auto end = static_cast<std::size_t>(ends[insertion.function]);
        move_right(terms, end, tail, shift);
        const std::span<QuadraticTerm> area = std::span(terms).subspan(begin, end - begin + shift);
        merge_back(area, end - begin, insertion.terms);
        shift_ends(ends, insertion.function, upto, shift);
        shift -= insertion.added;
        tail = begin;
        upto = insertion.function;
    }
}

std::size_t function_of(const Insertion &insertion) noexcept
{
    return insertion.function;
}

}

void merge_pairs(std::vector<Offset> &ends,
                 std::vector<QuadraticTerm> &terms,
                 std::span<const AddedPairs> additions)
{
    std::vector<Insertion> insertions;
    insertions.reserve(additions.size());
    for (const AddedPairs &addition : additions)
    {
        const auto function = static_cast<std::size_t>(addition.function);
        const std::size_t added = new_pairs(addition, held_by(ends, terms, function));
        insertions.push_back(Insertion{
            .function = function,
            .terms = addition.terms,
            .added = added,
        });
    }
    std::ranges::sort(insertions, {}, function_of);
    insert(ends, terms, insertions);
}

}
