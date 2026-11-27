#include "expr/sparsity/reach.hpp"

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/sparsity/joiner.hpp"
#include "expr/sparsity/memo.hpp"
#include "expr/sparsity/radix.hpp"
#include "expr/tape/plan.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

constexpr Offset kSeveral = -2;

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
    std::vector<Offset> met_heads_;
    std::vector<Offset> links_;
    std::vector<Index> own_names_;
    std::vector<Range> own_runs_;
    std::vector<Offset> stack_;
    Offset walk_ = 0;
};

Reacher::Reacher(const Graph &graph,
                 std::span<const Offset> positions,
                 std::span<const Offset> roots)
    : graph_(graph), positions_(positions), roots_(roots), labels_(to_size(graph.size()), kUnset),
      regions_(to_size(graph.size()), kUnset), heads_(to_size(graph.size()), false),
      seen_(to_size(graph.size()), false), chosen_(to_size(graph.size()), kUnset),
      names_of_(to_size(graph.size()), kUnset), runs_of_(to_size(graph.size()), kUnset)
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
        merge(labels_[to_size(roots_[r])], to_offset(r));
    }
    for (const Offset root : roots_)
    {
        heads_[to_size(root)] = true;
    }
    for (Offset node = graph_.size() - 1; node >= 0; --node)
    {
        if (labels_[to_size(node)] != kUnset)
        {
            divide(node);
        }
    }
}

void Reacher::divide(Offset node)
{
    const Offset label = labels_[to_size(node)];
    if (label == kSeveral && heads_[to_size(node)])
    {
        regions_[to_size(node)] = node;
    }
    heads_[to_size(node)] = label == kSeveral && regions_[to_size(node)] == node;
    for (const Offset child : graph_.children(node))
    {
        Offset &region = regions_[to_size(child)];
        merge(labels_[to_size(child)], label);
        if (label != kSeveral || (region != kUnset && region != regions_[to_size(node)]))
        {
            heads_[to_size(child)] = true;
        }
        if (region == kUnset && label == kSeveral)
        {
            region = regions_[to_size(node)];
        }
    }
}

void Reacher::remember()
{
    for (Offset node = 0; node < graph_.size(); ++node)
    {
        if (heads_[to_size(node)])
        {
            collect(node);
            names_of_[to_size(node)] = keep(names_, names_of_, variables_, own_names_);
            runs_of_[to_size(node)] = keep(runs_, runs_of_, pieces_, own_runs_);
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
    for (const Offset head : met_heads_)
    {
        links_.push_back(memo_of[to_size(head)]);
    }
    std::ranges::sort(links_);
    links_.erase(std::ranges::unique(links_).begin(), links_.end());
}

void Reacher::trace(Offset root, Reach &reach)
{
    if (heads_[to_size(root)])
    {
        variables_.clear();
        pieces_.clear();
        met_heads_.assign(1, root);
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
    reach.pattern.starts.push_back(to_offset(reach.pattern.variables.size()));
    reach.route.starts.push_back(to_offset(reach.route.runs.size()));
}

void Reacher::collect(Offset start)
{
    ++walk_;
    variables_.clear();
    pieces_.clear();
    met_heads_.clear();
    take(start);
    stack_.push_back(start);
    while (!stack_.empty())
    {
        const Offset node = stack_.back();
        stack_.pop_back();
        for (const Offset child : graph_.children(node))
        {
            if (heads_[to_size(child)])
            {
                choose(child);
            }
            else if (!seen_[to_size(child)])
            {
                take(child);
                stack_.push_back(child);
            }
        }
    }
}

void Reacher::take(Offset node)
{
    seen_[to_size(node)] = true;
    if (graph_.op(node) == Operator::Variable)
    {
        variables_.push_back(graph_.variable(node));
    }
    const Offset position = positions_[to_size(node)];
    if (position != kUnset)
    {
        pieces_.push_back({.begin = position, .end = position + 1});
    }
}

void Reacher::choose(Offset head)
{
    if (chosen_[to_size(head)] != walk_)
    {
        chosen_[to_size(head)] = walk_;
        met_heads_.push_back(head);
    }
}

}

Reach reach_of(const Graph &graph, std::span<const Offset> positions, std::span<const Offset> roots)
{
    return Reacher(graph, positions, roots).reach();
}

}
