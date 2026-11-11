#include "expr/intervals.hpp"

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/layout.hpp"
#include "expr/plan.hpp"
#include "expr/tape.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

constexpr Offset kOutput = 2;
constexpr std::size_t kNoWalk = (std::numeric_limits<std::size_t>::max)();

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

Offset count_of(std::size_t value) noexcept
{
    return static_cast<Offset>(value);
}

Interval point(std::size_t position) noexcept
{
    const auto begin = static_cast<std::uint32_t>(position);
    return {.begin = begin, .end = begin + 1};
}

}

Locator::Locator(const Graph &graph, const Layout &layout) : graph_(graph), layout_(layout) {}

void Locator::locate(std::span<const Offset> outputs, Block &block)
{
    const std::size_t size = layout_.nodes().size();
    count(outputs);
    joins_.assign(size, {.first = 0, .count = 0});
    seen_.assign(size, kNoWalk);
    runs_.clear();
    for (std::size_t position = 0; position < size; ++position)
    {
        if (uses_[position] != 1)
        {
            join(position);
        }
    }
    block.starts.push_back(0);
    for (const Offset output : outputs)
    {
        const std::span<const Interval> runs = runs_of(output);
        block.intervals.insert(block.intervals.end(), runs.begin(), runs.end());
        block.starts.push_back(count_of(block.intervals.size()));
    }
}

void Locator::count(std::span<const Offset> outputs)
{
    uses_.assign(layout_.nodes().size(), 0);
    for (const Offset node : layout_.nodes())
    {
        for (const Offset child : graph_.children(node))
        {
            use(child, 1);
        }
    }
    for (const Offset output : outputs)
    {
        use(output, kOutput);
    }
}

void Locator::use(Offset node, Offset times)
{
    const Offset position = layout_.position(node);
    if (position != kUnset)
    {
        uses_[at(position)] += times;
    }
}

void Locator::join(std::size_t position)
{
    pieces_.clear();
    pieces_.push_back(point(position));
    stack_.push_back(layout_.nodes()[position]);
    while (!stack_.empty())
    {
        const Offset node = stack_.back();
        stack_.pop_back();
        for (const Offset child : graph_.children(node))
        {
            enter(child, position);
        }
    }
    keep(position);
}

void Locator::enter(Offset node, std::size_t walk)
{
    const Offset position = layout_.position(node);
    if (position == kUnset)
    {
        return;
    }
    const std::size_t place = at(position);
    if (uses_[place] == 1)
    {
        pieces_.push_back(point(place));
        stack_.push_back(node);
        return;
    }
    if (seen_[place] != walk)
    {
        seen_[place] = walk;
        const std::span<const Interval> runs = runs_of(node);
        pieces_.insert(pieces_.end(), runs.begin(), runs.end());
    }
}

void Locator::keep(std::size_t position)
{
    std::ranges::sort(pieces_, {}, &Interval::begin);
    const std::size_t first = runs_.size();
    for (const Interval &piece : pieces_)
    {
        if (runs_.size() > first && piece.begin <= runs_.back().end)
        {
            runs_.back().end = (std::max)(runs_.back().end, piece.end);
            continue;
        }
        runs_.push_back(piece);
    }
    joins_[position] = {.first = count_of(first), .count = count_of(runs_.size() - first)};
}

std::span<const Interval> Locator::runs_of(Offset node) const noexcept
{
    const Offset position = layout_.position(node);
    if (position == kUnset)
    {
        return {};
    }
    const Runs runs = joins_[at(position)];
    return std::span(runs_).subspan(at(runs.first), at(runs.count));
}

}
