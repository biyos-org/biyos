#include "expr/reach.hpp"

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/memo.hpp"
#include "expr/node.hpp"
#include "expr/plan.hpp"
#include "expr/radix.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
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

class Reacher
{
  public:
    Reacher(const Graph &graph, std::span<const Offset> positions, std::span<const Offset> roots);

    Reach reach();

  private:
    void label();
    void divide(Offset node);
    void remember();
    template <typename Item>
    Offset keep(Memos<Item> &memos,
                const std::vector<Offset> &memo_of,
                std::vector<Item> &found,
                std::vector<Item> &own);
    void link(const std::vector<Offset> &memo_of);
    void trace(Offset root, Reach &reach);
    void collect(Offset start);
    void take(Offset node);
    void choose(Offset head);

    const Graph &graph_;
    std::span<const Offset> positions_;
    std::span<const Offset> roots_;
    std::vector<Offset> labels_;
    std::vector<Offset> regions_;
    std::vector<bool> heads_;
    std::vector<bool> seen_;
    std::vector<Offset> chosen_;
    std::vector<Offset> names_of_;
    std::vector<Offset> runs_of_;
    Memos<Index> names_;
    Memos<Range> runs_;
    Radix radix_;
    std::vector<Index> variables_;
    std::vector<Range> pieces_;
    std::vector<Offset> heads_met_;
    std::vector<Offset> links_;
    std::vector<Index> own_names_;
    std::vector<Range> own_runs_;
    std::vector<Offset> stack_;
    Offset walk_ = 0;
};

Reacher::Reacher(const Graph &graph,
                 std::span<const Offset> positions,
                 std::span<const Offset> roots)
    : graph_(graph), positions_(positions), roots_(roots), labels_(at(graph.size()), kUnset),
      regions_(at(graph.size()), kUnset), heads_(at(graph.size()), false),
      seen_(at(graph.size()), false), chosen_(at(graph.size()), kUnset),
      names_of_(at(graph.size()), kUnset), runs_of_(at(graph.size()), kUnset)
{
}

Reach Reacher::reach()
{
    label();
    remember();
    Reach reach{
        .route = {.runs = {}, .starts = {0}},
        .pattern = {.variables = {}, .starts = {0}},
    };
    for (const Offset root : roots_)
    {
        trace(root, reach);
    }
    return reach;
}

void Reacher::label()
{
    for (std::size_t r = 0; r < roots_.size(); ++r)
    {
        merge(labels_[at(roots_[r])], count_of(r));
    }
    for (const Offset root : roots_)
    {
        heads_[at(root)] = true;
    }
    for (Offset node = graph_.size() - 1; node >= 0; --node)
    {
        if (labels_[at(node)] != kUnset)
        {
            divide(node);
        }
    }
}

void Reacher::divide(Offset node)
{
    const Offset label = labels_[at(node)];
    if (label == kSeveral && heads_[at(node)])
    {
        regions_[at(node)] = node;
    }
    heads_[at(node)] = label == kSeveral && regions_[at(node)] == node;
    for (const Offset child : graph_.children(node))
    {
        Offset &region = regions_[at(child)];
        merge(labels_[at(child)], label);
        if (label != kSeveral || (region != kUnset && region != regions_[at(node)]))
        {
            heads_[at(child)] = true;
        }
        if (region == kUnset && label == kSeveral)
        {
            region = regions_[at(node)];
        }
    }
}

void Reacher::remember()
{
    for (Offset node = 0; node < graph_.size(); ++node)
    {
        if (heads_[at(node)])
        {
            collect(node);
            names_of_[at(node)] = keep(names_, names_of_, variables_, own_names_);
            runs_of_[at(node)] = keep(runs_, runs_of_, pieces_, own_runs_);
        }
    }
}

template <typename Item>
Offset Reacher::keep(Memos<Item> &memos,
                     const std::vector<Offset> &memo_of,
                     std::vector<Item> &found,
                     std::vector<Item> &own)
{
    own.clear();
    settle(found, own);
    link(memo_of);
    return memos.keep(own, links_);
}

void Reacher::link(const std::vector<Offset> &memo_of)
{
    links_.clear();
    for (const Offset head : heads_met_)
    {
        links_.push_back(memo_of[at(head)]);
    }
    std::ranges::sort(links_);
    links_.erase(std::ranges::unique(links_).begin(), links_.end());
}

void Reacher::trace(Offset root, Reach &reach)
{
    if (heads_[at(root)])
    {
        variables_.clear();
        pieces_.clear();
        heads_met_.assign(1, root);
    }
    else
    {
        collect(root);
    }
    link(names_of_);
    names_.gather(links_, variables_);
    link(runs_of_);
    runs_.gather(links_, pieces_);
    settle(variables_, reach.pattern.variables);
    radix_.sort(pieces_);
    join(pieces_, reach.route.runs);
    reach.pattern.starts.push_back(count_of(reach.pattern.variables.size()));
    reach.route.starts.push_back(count_of(reach.route.runs.size()));
}

void Reacher::collect(Offset start)
{
    ++walk_;
    variables_.clear();
    pieces_.clear();
    heads_met_.clear();
    take(start);
    stack_.push_back(start);
    while (!stack_.empty())
    {
        const Offset node = stack_.back();
        stack_.pop_back();
        for (const Offset child : graph_.children(node))
        {
            if (heads_[at(child)])
            {
                choose(child);
            }
            else if (!seen_[at(child)])
            {
                take(child);
                stack_.push_back(child);
            }
        }
    }
}

void Reacher::take(Offset node)
{
    seen_[at(node)] = true;
    if (graph_.op(node) == Operator::Variable)
    {
        variables_.push_back(graph_.variable(node));
    }
    const Offset position = positions_[at(node)];
    if (position != kUnset)
    {
        pieces_.push_back({.begin = position, .end = position + 1});
    }
}

void Reacher::choose(Offset head)
{
    if (chosen_[at(head)] != walk_)
    {
        chosen_[at(head)] = walk_;
        heads_met_.push_back(head);
    }
}

}

Reach reach_of(const Graph &graph, std::span<const Offset> positions, std::span<const Offset> roots)
{
    return Reacher(graph, positions, roots).reach();
}

}
