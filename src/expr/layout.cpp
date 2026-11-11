#include "expr/layout.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/plan.hpp"

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

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

Offset count_of(std::size_t value) noexcept
{
    return static_cast<Offset>(value);
}

}

std::uint32_t narrow(std::size_t count)
{
    if (count > kLimit)
    {
        throw Error(ErrorKind::NoMemory, "tape", "a block is too large for 32-bit slots");
    }
    return static_cast<std::uint32_t>(count);
}

Layout::Layout(const Graph &graph, const Plan &plan)
    : graph_(graph), plan_(plan), stamps_(at(graph.size()), kUnset),
      local_(at(graph.size()), kUnset)
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
    return id < loads_.size() ? kUnset : count_of(id - loads_.size());
}

std::uint32_t Layout::width() const noexcept
{
    return width_;
}

bool Layout::computed(Offset node, Offset block) const noexcept
{
    return graph_.children(node).empty() || plan_.home[at(node)] == block;
}

void Layout::enter(Offset node, Offset block)
{
    if (stamps_[at(node)] == block)
    {
        return;
    }
    stamps_[at(node)] = block;
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
        local_[at(loads_[k])] = count_of(k);
    }
    for (std::size_t k = 0; k < nodes_.size(); ++k)
    {
        local_[at(nodes_[k])] = count_of(loads_.size() + k);
    }
}

void Layout::mark(std::span<const Offset> outputs)
{
    last_.assign(loads_.size() + nodes_.size(), kUnset);
    for (std::size_t k = 0; k < nodes_.size(); ++k)
    {
        for (const Offset child : graph_.children(nodes_[k]))
        {
            last_[local(child)] = count_of(k);
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
            release(child, count_of(k));
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
    return at(local_[at(node)]);
}

}
