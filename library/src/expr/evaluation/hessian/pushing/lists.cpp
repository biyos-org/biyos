#include "expr/evaluation/hessian/pushing/lists.hpp"

#include "core/numbers.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/evaluation/hessian/pattern.hpp"
#include "expr/tape/layout.hpp"

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

std::uint32_t bends_in(const Node &node) noexcept
{
    const std::uint32_t n = node.end - node.begin;
    switch (node.bending)
    {
    case Bending::None:
        return 0;
    case Bending::Full:
        return n * (n + 1) / 2;
    case Bending::Quotient:
        return 2;
    case Bending::Cross:
        return n * (n - 1) / 2;
    }
    return 0;
}

std::vector<std::uint32_t> bend_starts(std::span<const Node> nodes)
{
    std::vector<std::uint32_t> starts{0};
    for (const Node &node : nodes)
    {
        starts.push_back(starts.back() + bends_in(node));
    }
    return starts;
}

std::vector<Offset> list_starts(std::span<const Offset> links)
{
    std::vector<Offset> starts{0};
    for (const Offset count : links)
    {
        starts.push_back(starts.back() + count);
    }
    return starts;
}

void index_rows(std::span<const Entry> entries, Elimination &elimination)
{
    std::vector<Offset> &starts = elimination.starts;
    starts.assign(std::size_t{elimination.variables} + 1, 0);
    for (const Entry entry : entries)
    {
        ++starts[to_size(entry.row) + 1];
    }
    std::partial_sum(starts.begin(), starts.end(), starts.begin());
    std::vector<Offset> next(starts.begin(), starts.end() - 1);
    elimination.columns.assign(entries.size(), 0);
    elimination.parts.assign(entries.size(), 0);
    for (std::size_t k = 0; k < entries.size(); ++k)
    {
        const auto at = to_size(next[to_size(entries[k].row)]++);
        elimination.columns[at] = narrow(to_size(entries[k].column));
        elimination.parts[at] = to_offset(k);
    }
}

}

Elimination elimination_of(Offset element, Shape shape)
{
    std::vector<std::uint32_t> bends = bend_starts(shape.nodes);
    std::vector<Offset> lists = list_starts(shape.links);
    Elimination elimination{
        .element = element,
        .variables = narrow(shape.links.size() - shape.nodes.size()),
        .root = shape.root,
        .nodes = std::move(shape.nodes),
        .operands = std::move(shape.operands),
        .bends = std::move(bends),
        .lists = std::move(lists),
        .starts = {},
        .columns = {},
        .parts = {},
    };
    index_rows(shape.entries, elimination);
    return elimination;
}

}
