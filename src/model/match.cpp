#include "model/match.hpp"

#include "core/index.hpp"
#include "core/text/text.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "model/fields.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace biyos
{

namespace
{

constexpr Offset kUnmatched = -1;

std::size_t at(Offset node) noexcept
{
    return static_cast<std::size_t>(node);
}

std::string node_pair(Offset a, Offset b)
{
    return "node " + text_of(a) + " against " + text_of(b);
}

int code_of(Operator op) noexcept
{
    return static_cast<int>(op);
}

std::string weight_differences(std::span<const double> a, std::span<const double> b)
{
    for (std::size_t k = 0; k < a.size(); ++k)
    {
        const std::string found = compared("weight", a[k], b[k]);
        if (!found.empty())
        {
            return "child " + text_of(k) + ": " + found;
        }
    }
    return "";
}

std::string payload_difference(const Graph &a, const Graph &b, Offset x, Offset y)
{
    switch (payload_of(a.op(x)))
    {
    case Payload::None:
        return "";
    case Payload::Constant:
        return compared("constant", a.constant(x), b.constant(y));
    case Payload::Variable:
        return compared("variable", a.variable(x), b.variable(y));
    case Payload::Coefficients:
        return first_of({
            compared("constant", a.constant(x), b.constant(y)),
            weight_differences(a.coefficients(x), b.coefficients(y)),
        });
    }
    return "";
}

}

Matching::Matching(const Graph &a, const Graph &b)
    : a_(a), b_(b), to_b_(at(a.size()), kUnmatched), to_a_(at(b.size()), kUnmatched)
{
}

std::string Matching::difference(Offset a, Offset b)
{
    pending_.assign(1, {a, b});
    while (!pending_.empty())
    {
        const auto [x, y] = pending_.back();
        pending_.pop_back();
        const std::string found = visit(x, y);
        if (!found.empty())
        {
            return found;
        }
    }
    return "";
}

std::string Matching::visit(Offset a, Offset b)
{
    const Offset paired = to_b_[at(a)];
    const Offset partner = to_a_[at(b)];
    if (paired == b && partner == a)
    {
        return "";
    }
    if (paired != kUnmatched)
    {
        return node_pair(a, b) + ": shared as " + node_pair(a, paired);
    }
    if (partner != kUnmatched)
    {
        return node_pair(a, b) + ": shared as " + node_pair(partner, b);
    }
    const std::string found = node_difference(a, b);
    if (!found.empty())
    {
        return node_pair(a, b) + ": " + found;
    }
    to_b_[at(a)] = b;
    to_a_[at(b)] = a;
    const std::span<const Offset> left = a_.children(a);
    const std::span<const Offset> right = b_.children(b);
    for (std::size_t k = left.size(); k > 0; --k)
    {
        pending_.emplace_back(left[k - 1], right[k - 1]);
    }
    return "";
}

std::string Matching::node_difference(Offset a, Offset b) const
{
    const Operator op = a_.op(a);
    if (op != b_.op(b))
    {
        return against("operator", code_of(op), code_of(b_.op(b)));
    }
    const auto n = static_cast<Offset>(a_.children(a).size());
    const auto m = static_cast<Offset>(b_.children(b).size());
    if (n != m)
    {
        return against("child count", n, m);
    }
    return payload_difference(a_, b_, a, b);
}

}
