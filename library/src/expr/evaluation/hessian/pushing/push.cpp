#include "expr/evaluation/hessian/pushing/push.hpp"

#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/numeric/zero.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/evaluation/hessian/pattern.hpp"
#include "expr/evaluation/hessian/pushing/lists.hpp"
#include "expr/evaluation/hessian/pushing/walk.hpp"
#include "expr/evaluation/hessian/tangents.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

constexpr Offset kNone = -1;

struct Widest
{
    std::size_t capacity;
    std::size_t ids;
    std::size_t operands;
    std::size_t bends;
};

struct Push
{
    const Elimination &elimination;
    PushSpace &space;
};

Widest widest_of(std::span<const Elimination> eliminations) noexcept
{
    Widest widest{.capacity = 0, .ids = 0, .operands = 0, .bends = 0};
    for (const Elimination &elimination : eliminations)
    {
        widest.capacity = (std::max)(widest.capacity, to_size(elimination.lists.back()));
        widest.ids = (std::max)(widest.ids, elimination.lists.size() - 1);
        widest.operands = (std::max)(widest.operands, elimination.operands.size());
        widest.bends = (std::max)(widest.bends, std::size_t{elimination.bends.back()});
    }
    return widest;
}

Real times(Real a, Real b) noexcept
{
    return is_zero(a) || is_zero(b) ? 0.0 : a * b;
}

void link(const Push &push, std::uint32_t a, std::uint32_t b, Real value, bool across) noexcept
{
    if (is_zero(value))
    {
        return;
    }
    const std::uint32_t row = (std::max)(a, b);
    const auto at = to_size(push.elimination.lists[row] + push.space.sizes[row]++);
    push.space.columns[at] = (std::min)(a, b);
    push.space.values[at] = across && a == b ? 2.0 * value : value;
}

Offset merge(const Push &push, std::size_t row) noexcept
{
    PushSpace &space = push.space;
    const Offset begin = push.elimination.lists[row];
    const Offset end = begin + std::exchange(space.sizes[row], 0);
    Offset merged = begin;
    for (Offset k = begin; k < end; ++k)
    {
        const std::uint32_t column = space.columns[to_size(k)];
        Offset &mark = space.marks[column];
        if (mark == kNone)
        {
            mark = merged;
            space.columns[to_size(merged)] = column;
            space.values[to_size(merged)] = space.values[to_size(k)];
            ++merged;
        }
        else
        {
            space.values[to_size(mark)] += space.values[to_size(k)];
        }
    }
    for (Offset k = begin; k < merged; ++k)
    {
        space.marks[space.columns[to_size(k)]] = kNone;
    }
    return merged;
}

void spread(const Push &push,
            std::span<const std::uint32_t> operands,
            std::span<const Real> slopes,
            std::uint32_t neighbour,
            Real weight) noexcept
{
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        link(push, operands[k], neighbour, times(weight, slopes[k]), true);
    }
}

void square(const Push &push,
            std::span<const std::uint32_t> operands,
            std::span<const Real> slopes,
            Real weight) noexcept
{
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        const Real along = times(weight, slopes[k]);
        for (std::size_t m = k; m < operands.size() && !is_zero(along); ++m)
        {
            link(push, operands[k], operands[m], times(along, slopes[m]), k != m);
        }
    }
}

void create(const Push &push,
            const Node &node,
            std::span<const std::uint32_t> operands,
            std::span<const Real> bends,
            Real adjoint) noexcept
{
    std::size_t at = 0;
    switch (node.bending)
    {
    case Bending::None:
        return;
    case Bending::Full:
        for (std::size_t k = 0; k < operands.size(); ++k)
        {
            for (std::size_t m = k; m < operands.size(); ++m)
            {
                link(push, operands[k], operands[m], scaled(adjoint, bends[at++]), k != m);
            }
        }
        return;
    case Bending::Quotient:
        link(push, operands.front(), operands.back(), scaled(adjoint, bends[0]), true);
        link(push, operands.back(), operands.back(), scaled(adjoint, bends[1]), false);
        return;
    case Bending::Cross:
        for (std::size_t k = 0; k < operands.size(); ++k)
        {
            for (std::size_t m = k + 1; m < operands.size(); ++m)
            {
                link(push, operands[k], operands[m], scaled(adjoint, bends[at++]), true);
            }
        }
        return;
    }
}

void eliminate(const Push &push, std::size_t n) noexcept
{
    const Elimination &elimination = push.elimination;
    PushSpace &space = push.space;
    const Node &node = elimination.nodes[n];
    const std::size_t id = elimination.variables + n;
    const std::size_t count = node.end - node.begin;
    const std::span<const std::uint32_t> operands =
        std::span(elimination.operands).subspan(node.begin, count);
    const std::span<const Real> slopes = std::span(space.slopes).subspan(node.begin, count);
    const Offset end = merge(push, id);
    for (auto k = to_size(elimination.lists[id]); k < to_size(end); ++k)
    {
        const std::uint32_t neighbour = space.columns[k];
        const Real weight = space.values[k];
        if (neighbour == id)
        {
            square(push, operands, slopes, weight);
        }
        else
        {
            spread(push, operands, slopes, neighbour, weight);
        }
    }
    const Real adjoint = std::exchange(space.adjoints[id], 0.0);
    if (is_zero(adjoint))
    {
        return;
    }
    for (std::size_t k = 0; k < count; ++k)
    {
        space.adjoints[operands[k]] += times(adjoint, slopes[k]);
    }
    const std::uint32_t first = elimination.bends[n];
    const std::span<const Real> bends =
        std::span(space.bends).subspan(first, elimination.bends[n + 1] - first);
    create(push, node, operands, bends, adjoint);
}

void scatter(const Push &push, std::uint32_t row, std::span<Real> parts) noexcept
{
    const Elimination &elimination = push.elimination;
    PushSpace &space = push.space;
    const std::span<const std::uint32_t> columns =
        slice(std::span(elimination.columns), elimination.starts, row);
    const std::span<const Offset> places =
        slice(std::span(elimination.parts), elimination.starts, row);
    for (std::size_t k = 0; k < columns.size(); ++k)
    {
        space.marks[columns[k]] = places[k];
    }
    const auto begin = to_size(elimination.lists[row]);
    const auto end = begin + to_size(std::exchange(space.sizes[row], 0));
    for (std::size_t k = begin; k < end; ++k)
    {
        parts[to_size(space.marks[space.columns[k]])] += space.values[k];
    }
    for (const std::uint32_t column : columns)
    {
        space.marks[column] = kNone;
    }
    space.adjoints[row] = 0.0;
}

}

PushSpace space_of(const Tape &tape, std::span<const Elimination> eliminations)
{
    const Widest widest = widest_of(eliminations);
    return {
        .columns = std::vector<std::uint32_t>(widest.capacity, 0),
        .values = std::vector<Real>(widest.capacity, 0.0),
        .sizes = std::vector<Offset>(widest.ids, 0),
        .marks = std::vector<Offset>(widest.ids, kNone),
        .adjoints = std::vector<Real>(widest.ids, 0.0),
        .slopes = std::vector<Real>(widest.operands, 0.0),
        .bends = std::vector<Real>(widest.bends, 0.0),
        .suffix = std::vector<Real>(widest_product(tape) + 1, 0.0),
    };
}

void push(const Tape &tape,
          const Elimination &elimination,
          std::span<const Real> point,
          const Tolerances &tolerances,
          Workspace &workspace,
          PushSpace &space,
          std::span<Real> parts) noexcept
{
    walk_element(tape, elimination, point, tolerances, workspace, space);
    std::ranges::fill(parts, 0.0);
    const Push pushing{.elimination = elimination, .space = space};
    space.adjoints[elimination.root] = 1.0;
    for (std::size_t n = elimination.nodes.size(); n > 0; --n)
    {
        eliminate(pushing, n - 1);
    }
    for (std::uint32_t row = 0; row < elimination.variables; ++row)
    {
        scatter(pushing, row, parts);
    }
}

}
