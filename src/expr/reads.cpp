#include "expr/reads.hpp"

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/layout.hpp"
#include "expr/plan.hpp"
#include "expr/reach.hpp"
#include "expr/tape.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

Offset count_of(std::size_t value) noexcept
{
    return static_cast<Offset>(value);
}

void add(Block &block, Run run)
{
    const bool joined = count_of(block.reads.size()) > block.read_starts.back();
    if (joined && block.reads.back().end == run.begin)
    {
        block.reads.back().end = run.end;
        return;
    }
    block.reads.push_back(run);
}

std::uint32_t slot_of(std::span<const Offset> loads, Offset position)
{
    const auto found = std::ranges::lower_bound(loads, position);
    return static_cast<std::uint32_t>(found - loads.begin());
}

std::span<const Range> route_of(const Reach &reach, Offset root)
{
    const Offset begin = reach.route.starts[at(root)];
    const Offset end = reach.route.starts[at(root) + 1];
    return std::span(reach.route.runs).subspan(at(begin), at(end - begin));
}

}

void read_children(const Graph &graph,
                   const Layout &layout,
                   std::span<const Offset> outputs,
                   Block &block)
{
    std::vector<std::uint32_t> slots;
    block.read_starts.push_back(0);
    for (const Offset output : outputs)
    {
        slots.clear();
        for (const Offset child : graph.children(output))
        {
            if (layout.position(child) == kUnset)
            {
                slots.push_back(layout.slot(child));
            }
        }
        std::ranges::sort(slots);
        const auto last = std::ranges::unique(slots).begin();
        for (const std::uint32_t slot : std::span(slots.begin(), last))
        {
            add(block, {.begin = slot, .end = slot + 1});
        }
        block.read_starts.push_back(count_of(block.reads.size()));
    }
}

void read_routes(const Reach &reach, Block &block)
{
    block.read_starts.push_back(0);
    for (std::size_t k = 0; k < block.outputs.size(); ++k)
    {
        for (const Range run : route_of(reach, block.first + count_of(k)))
        {
            const std::uint32_t begin = slot_of(block.loads, run.begin);
            const std::uint32_t end = slot_of(block.loads, run.end);
            if (begin < end)
            {
                add(block, {.begin = begin, .end = end});
            }
        }
        block.read_starts.push_back(count_of(block.reads.size()));
    }
}

}
