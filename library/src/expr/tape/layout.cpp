#include "expr/tape/layout.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/tape/plan.hpp"

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

constexpr Offset kHeld = (std::numeric_limits<Offset>::max)();
constexpr std::size_t kLimit = (std::numeric_limits<std::uint32_t>::max)();

}

std::uint32_t narrow(std::size_t n)
{
    if (n > kLimit)
    {
        throw Error(ErrorKind::NoMemory, "tape", "a block is too large for 32-bit slots");
    }
    return static_cast<std::uint32_t>(n);
}

Layout::Layout(const Graph &graph, const Plan &plan)
    : graph_(graph), plan_(plan), stamps_(to_size(graph.size()), kUnset),
      local_(to_size(graph.size()), kUnset)
{
}

void Layout::build(Offset block, std::span<const Offset> outputs)
{
    collect(block, outputs);
    number();
    mark(outputs);
    allocate();
}

std::span<const Offset> Layout::nodes() const noexcept
{
    return nodes_;
}

std::span<const Offset> Layout::loads() const noexcept
{
    return loads_;
}

std::uint32_t Layout::slot(Offset node) const noexcept
{
    return slots_[local(node)];
}

Offset Layout::position(Offset node) const noexcept
{
    const std::size_t id = local(node);
    return id < loads_.size() ? kUnset : to_offset(id - loads_.size());
}

std::uint32_t Layout::width() const noexcept
{
    return width_;
}

bool Layout::computed(Offset node, Offset block) const noexcept
{
    return graph_.children(node).empty() || plan_.home[to_size(node)] == block;
}

void Layout::enter(Offset node, Offset block)
{
    if (stamps_[to_size(node)] == block)
    {
        return;
    }
    stamps_[to_size(node)] = block;
    if (!computed(node, block))
    {
        loads_.push_back(node);
        return;
    }
    stack_.push_back({.node = node, .next = 0});
}

void Layout::descend(Offset output, Offset block)
{
    enter(output, block);
    while (!stack_.empty())
    {
        const Step step = stack_.back();
        const std::span<const Offset> children = graph_.children(step.node);
        if (step.next == children.size())
        {
            nodes_.push_back(step.node);
            stack_.pop_back();
            continue;
        }
        ++stack_.back().next;
        enter(children[step.next], block);
    }
}

void Layout::collect(Offset block, std::span<const Offset> outputs)
{
    nodes_.clear();
    loads_.clear();
    for (const Offset output : outputs)
    {
        descend(output, block);
    }
    std::ranges::sort(loads_);
}

void Layout::number()
{
    narrow(loads_.size() + nodes_.size());
    for (std::size_t k = 0; k < loads_.size(); ++k)
    {
        local_[to_size(loads_[k])] = to_offset(k);
    }
    for (std::size_t k = 0; k < nodes_.size(); ++k)
    {
        local_[to_size(nodes_[k])] = to_offset(loads_.size() + k);
    }
}

void Layout::mark(std::span<const Offset> outputs)
{
    last_.assign(loads_.size() + nodes_.size(), kUnset);
    for (std::size_t k = 0; k < nodes_.size(); ++k)
    {
        for (const Offset child : graph_.children(nodes_[k]))
        {
            last_[local(child)] = to_offset(k);
        }
    }
    for (const Offset output : outputs)
    {
        last_[local(output)] = kHeld;
    }
}

void Layout::allocate()
{
    free_.clear();
    width_ = 0;
    slots_.assign(loads_.size() + nodes_.size(), 0);
    for (std::size_t k = 0; k < loads_.size(); ++k)
    {
        slots_[k] = take();
    }
    for (std::size_t k = 0; k < nodes_.size(); ++k)
    {
        for (const Offset child : graph_.children(nodes_[k]))
        {
            release(child, to_offset(k));
        }
        slots_[loads_.size() + k] = take();
    }
}

void Layout::release(Offset node, Offset use)
{
    Offset &last = last_[local(node)];
    if (last == use)
    {
        free_.push_back(slot(node));
        last = kUnset;
    }
}

std::uint32_t Layout::take()
{
    if (free_.empty())
    {
        return width_++;
    }
    const std::uint32_t reused = free_.back();
    free_.pop_back();
    return reused;
}

std::size_t Layout::local(Offset node) const noexcept
{
    return to_size(local_[to_size(node)]);
}

}
