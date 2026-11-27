#include "expr/evaluation/hessian/elements.hpp"

#include "core/env/error.hpp"
#include "core/memory/slice.hpp"
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
    Real weight;
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
        const std::span<const Real> coefficients = graph.coefficients(term.node);
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

Entry global_of(std::span<const Index> variables, Entry entry) noexcept
{
    return {.row = variables[to_size(entry.row)], .column = variables[to_size(entry.column)]};
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

std::vector<Key> keys_of(const Pattern &pattern,
                         const Blocks &blocks,
                         std::span<const Entry> declared)
{
    std::vector<Key> keys;
    for (std::size_t e = 0; e + 1 < blocks.starts.size(); ++e)
    {
        const std::span<const Index> variables = variables_of(pattern, to_offset(e));
        for (const Entry entry : entries_of(blocks, to_offset(e)))
        {
            keys.push_back(key_of(global_of(variables, entry)));
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

Index columns_of(const Pattern &pattern, std::span<const Entry> declared) noexcept
{
    Index size = pattern.variables.empty() ? 0 : std::ranges::max(pattern.variables) + 1;
    for (const Entry entry : declared)
    {
        size = (std::max)(size, lower_entry(entry).row + 1);
    }
    return size;
}

}

std::span<const Index> variables_of(const Pattern &pattern, Offset element) noexcept
{
    return slice(std::span(pattern.variables), pattern.starts, to_size(element));
}

std::span<const Entry> entries_of(const Blocks &blocks, Offset element) noexcept
{
    return slice(std::span(blocks.entries), blocks.starts, to_size(element));
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

void add_dense(std::size_t size, Blocks &blocks)
{
    for (std::size_t column = 0; column < size; ++column)
    {
        for (std::size_t row = column; row < size; ++row)
        {
            blocks.entries.push_back({.row = to_index(row), .column = to_index(column)});
        }
    }
    blocks.starts.push_back(to_offset(blocks.entries.size()));
}

Blocks dense_of(const Pattern &pattern)
{
    Blocks blocks{.starts = {0}, .entries = {}};
    for (std::size_t e = 0; e + 1 < pattern.starts.size(); ++e)
    {
        add_dense(variables_of(pattern, to_offset(e)).size(), blocks);
    }
    return blocks;
}

Lower lower_of(const Pattern &pattern, const Blocks &blocks, std::span<const Entry> declared)
{
    check_declared(declared);
    const std::vector<Key> keys = keys_of(pattern, blocks, declared);
    Lower lower{
        .starts = std::vector<Offset>(to_size(columns_of(pattern, declared)) + 1, 0),
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
    const auto at = to_size(entry.column);
    const std::span<const Index> rows = slice(std::span(lower.rows), lower.starts, at);
    const auto found = std::ranges::lower_bound(rows, entry.row);
    return lower.starts[at] + static_cast<Offset>(found - rows.begin());
}

Map map_of(const Lower &lower,
           const Pattern &pattern,
           const Blocks &blocks,
           std::span<const Entry> declared)
{
    Map map;
    map.starts.push_back(0);
    for (std::size_t e = 0; e + 1 < blocks.starts.size(); ++e)
    {
        const std::span<const Index> variables = variables_of(pattern, to_offset(e));
        for (const Entry entry : entries_of(blocks, to_offset(e)))
        {
            const Entry global = global_of(variables, entry);
            map.blocks.push_back(position(lower, global.row, global.column));
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
