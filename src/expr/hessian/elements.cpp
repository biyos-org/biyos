#include "expr/hessian/elements.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/sparsity/reach.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

struct Term
{
    Offset node;
    double weight;
};

using Key = std::uint64_t;

void flatten(const Graph &graph, Offset root, Offset index, Elements &elements)
{
    std::vector<Term> stack{{.node = root, .weight = 1.0}};
    while (!stack.empty())
    {
        const Term term = stack.back();
        stack.pop_back();
        if (graph.op(term.node) != Operator::Sum)
        {
            elements.nodes.push_back(term.node);
            elements.roots.push_back(index);
            elements.weights.push_back(term.weight);
            continue;
        }
        const std::span<const Offset> children = graph.children(term.node);
        const std::span<const double> coefficients = graph.coefficients(term.node);
        for (std::size_t k = children.size(); k > 0; --k)
        {
            stack.push_back({.node = children[k - 1], .weight = term.weight * coefficients[k - 1]});
        }
    }
}

Entry lower_entry(Entry entry) noexcept
{
    return {.row = (std::max)(entry.row, entry.column),
            .column = (std::min)(entry.row, entry.column)};
}

Key key_of(Entry entry) noexcept
{
    const Entry lower = lower_entry(entry);
    return (Key{static_cast<std::uint32_t>(lower.column)} << 32) |
           static_cast<std::uint32_t>(lower.row);
}

Entry entry_of(Key key) noexcept
{
    return {.row = static_cast<Index>(key & 0xffffffffU), .column = static_cast<Index>(key >> 32)};
}

std::span<const Index> row_of(const Pattern &blocks, std::size_t element) noexcept
{
    const Offset begin = blocks.starts[element];
    return std::span(blocks.variables)
        .subspan(to_size(begin), to_size(blocks.starts[element + 1] - begin));
}

void check_declared(std::span<const Entry> declared)
{
    for (std::size_t k = 0; k < declared.size(); ++k)
    {
        if (declared[k].row < 0 || declared[k].column < 0)
        {
            throw Error(ErrorKind::InvalidArgument,
                        "entries",
                        "entry " + text_of(k) + " has a negative index");
        }
    }
}

std::vector<Key> keys_of(const Pattern &blocks, std::span<const Entry> declared)
{
    std::vector<Key> keys;
    for (std::size_t e = 0; e + 1 < blocks.starts.size(); ++e)
    {
        const std::span<const Index> variables = row_of(blocks, e);
        for (std::size_t l = 0; l < variables.size(); ++l)
        {
            for (std::size_t a = l; a < variables.size(); ++a)
            {
                keys.push_back(key_of({.row = variables[a], .column = variables[l]}));
            }
        }
    }
    for (const Entry entry : declared)
    {
        keys.push_back(key_of(entry));
    }
    std::ranges::sort(keys);
    keys.erase(std::ranges::unique(keys).begin(), keys.end());
    return keys;
}

Index columns_of(const Pattern &blocks, std::span<const Entry> declared) noexcept
{
    Index size = blocks.variables.empty() ? 0 : std::ranges::max(blocks.variables) + 1;
    for (const Entry entry : declared)
    {
        size = (std::max)(size, lower_entry(entry).row + 1);
    }
    return size;
}

}

Elements elements_of(const Graph &graph, std::span<const Offset> roots)
{
    Elements elements;
    for (std::size_t r = 0; r < roots.size(); ++r)
    {
        check_child(roots[r], graph.size());
        flatten(graph, roots[r], to_offset(r), elements);
    }
    return elements;
}

Lower lower_of(const Pattern &blocks, std::span<const Entry> declared)
{
    check_declared(declared);
    const std::vector<Key> keys = keys_of(blocks, declared);
    Lower lower{
        .starts = std::vector<Offset>(to_size(columns_of(blocks, declared)) + 1, 0),
        .rows = {},
    };
    lower.rows.reserve(keys.size());
    for (const Key key : keys)
    {
        const Entry entry = entry_of(key);
        ++lower.starts[to_size(entry.column) + 1];
        lower.rows.push_back(entry.row);
    }
    for (std::size_t k = 1; k < lower.starts.size(); ++k)
    {
        lower.starts[k] += lower.starts[k - 1];
    }
    return lower;
}

Offset position(const Lower &lower, Index row, Index column) noexcept
{
    const Entry entry = lower_entry({.row = row, .column = column});
    const Offset begin = lower.starts[to_size(entry.column)];
    const Offset end = lower.starts[to_size(entry.column) + 1];
    const std::span<const Index> rows =
        std::span(lower.rows).subspan(to_size(begin), to_size(end - begin));
    const auto found = std::ranges::lower_bound(rows, entry.row);
    return begin + static_cast<Offset>(found - rows.begin());
}

Map map_of(const Lower &lower, const Pattern &blocks, std::span<const Entry> declared)
{
    Map map;
    map.starts.push_back(0);
    for (std::size_t e = 0; e + 1 < blocks.starts.size(); ++e)
    {
        const std::span<const Index> variables = row_of(blocks, e);
        for (std::size_t l = 0; l < variables.size(); ++l)
        {
            for (std::size_t a = l; a < variables.size(); ++a)
            {
                map.blocks.push_back(position(lower, variables[a], variables[l]));
            }
        }
        map.starts.push_back(to_offset(map.blocks.size()));
    }
    for (const Entry entry : declared)
    {
        map.declared.push_back(position(lower, entry.row, entry.column));
    }
    return map;
}

}
