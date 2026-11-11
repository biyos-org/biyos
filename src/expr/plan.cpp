#include "expr/plan.hpp"

#include "core/index.hpp"
#include "expr/graph.hpp"

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

constexpr Offset kSeveral = -2;

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

Offset count_of(std::size_t value) noexcept
{
    return static_cast<Offset>(value);
}

bool is_leaf(const Graph &graph, Offset node) noexcept
{
    return graph.children(node).empty();
}

void merge(Offset &label, Offset other) noexcept
{
    if (label == kUnset)
    {
        label = other;
    }
    else if (label != other)
    {
        label = kSeveral;
    }
}

class Reach
{
  public:
    explicit Reach(const Graph &graph);

    std::span<const Offset> fresh(Offset root);

  private:
    void visit(Offset node);

    const Graph &graph_;
    std::vector<bool> reached_;
    std::vector<Offset> stack_;
    std::vector<Offset> fresh_;
};

Reach::Reach(const Graph &graph) : graph_(graph), reached_(at(graph.size()), false) {}

std::span<const Offset> Reach::fresh(Offset root)
{
    fresh_.clear();
    visit(root);
    while (!stack_.empty())
    {
        const Offset node = stack_.back();
        stack_.pop_back();
        for (const Offset child : graph_.children(node))
        {
            visit(child);
        }
    }
    return fresh_;
}

void Reach::visit(Offset node)
{
    if (is_leaf(graph_, node) || reached_[at(node)])
    {
        return;
    }
    reached_[at(node)] = true;
    fresh_.push_back(node);
    stack_.push_back(node);
}

class Cut
{
  public:
    Cut(const Graph &graph, Offset budget);

    Offset add(Offset node, std::span<const Offset> fresh, Offset item);
    std::vector<Offset> finish(Offset items);

  private:
    Offset weight(Offset node, std::span<const Offset> fresh, Offset block);
    Offset stamp(Offset node, Offset block);

    const Graph &graph_;
    Offset budget_;
    Offset load_ = 0;
    std::vector<Offset> firsts_;
    std::vector<Offset> stamps_;
};

Cut::Cut(const Graph &graph, Offset budget)
    : graph_(graph), budget_(budget), firsts_(1, 0), stamps_(at(graph.size()), kUnset)
{
}

Offset Cut::add(Offset node, std::span<const Offset> fresh, Offset item)
{
    const Offset block = count_of(firsts_.size()) - 1;
    Offset added = weight(node, fresh, block);
    if (load_ > 0 && load_ + added > budget_)
    {
        firsts_.push_back(item);
        added = weight(node, fresh, block + 1);
        load_ = 0;
    }
    load_ += added;
    return count_of(firsts_.size()) - 1;
}

std::vector<Offset> Cut::finish(Offset items)
{
    if (items > 0)
    {
        firsts_.push_back(items);
    }
    return std::move(firsts_);
}

Offset Cut::weight(Offset node, std::span<const Offset> fresh, Offset block)
{
    Offset total = count_of(fresh.size()) + stamp(node, block);
    for (const Offset parent : fresh)
    {
        for (const Offset child : graph_.children(parent))
        {
            total += stamp(child, block);
        }
    }
    return total;
}

Offset Cut::stamp(Offset node, Offset block)
{
    if (!is_leaf(graph_, node) || stamps_[at(node)] == block)
    {
        return 0;
    }
    stamps_[at(node)] = block;
    return 1;
}

std::vector<Offset> cut_roots(const Graph &graph, std::span<const Offset> roots, Offset budget)
{
    Reach reach(graph);
    Cut cut(graph, budget);
    for (std::size_t r = 0; r < roots.size(); ++r)
    {
        cut.add(roots[r], reach.fresh(roots[r]), count_of(r));
    }
    return cut.finish(count_of(roots.size()));
}

std::vector<Offset> labels_of(const Graph &graph,
                              std::span<const Offset> roots,
                              std::span<const Offset> firsts)
{
    std::vector<Offset> labels(at(graph.size()), kUnset);
    for (std::size_t p = 0; p + 1 < firsts.size(); ++p)
    {
        for (Offset r = firsts[p]; r < firsts[p + 1]; ++r)
        {
            merge(labels[at(roots[at(r)])], count_of(p));
        }
    }
    for (Offset node = graph.size() - 1; node >= 0; --node)
    {
        const Offset label = labels[at(node)];
        if (label == kUnset)
        {
            continue;
        }
        for (const Offset child : graph.children(node))
        {
            merge(labels[at(child)], label);
        }
    }
    return labels;
}

void pin(const Graph &graph, Offset budget, Plan &plan)
{
    Cut cut(graph, budget);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        if (is_leaf(graph, node) || plan.home[at(node)] != kSeveral)
        {
            continue;
        }
        const Offset position = count_of(plan.pinned.size());
        plan.home[at(node)] = cut.add(node, std::span(&node, 1), position);
        plan.position[at(node)] = position;
        plan.pinned.push_back(node);
    }
    plan.prepass = cut.finish(count_of(plan.pinned.size()));
}

void rehome(const Graph &graph, Plan &plan)
{
    const Offset prepass = count_of(plan.prepass.size()) - 1;
    for (Offset node = 0; node < graph.size(); ++node)
    {
        Offset &home = plan.home[at(node)];
        if (is_leaf(graph, node))
        {
            home = kUnset;
        }
        else if (home != kUnset && plan.position[at(node)] == kUnset)
        {
            home += prepass;
        }
    }
}

}

Plan plan_of(const Graph &graph, std::span<const Offset> roots, Offset budget)
{
    Plan plan;
    plan.partitions = cut_roots(graph, roots, budget);
    plan.home = labels_of(graph, roots, plan.partitions);
    plan.position.assign(at(graph.size()), kUnset);
    pin(graph, budget, plan);
    rehome(graph, plan);
    return plan;
}

}
