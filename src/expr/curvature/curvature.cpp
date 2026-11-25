#include "expr/curvature/curvature.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/curvature/arithmetic.hpp"
#include "expr/curvature/bends.hpp"
#include "expr/curvature/exponents.hpp"
#include "expr/curvature/segments.hpp"
#include "expr/curvature/shape.hpp"
#include "expr/curvature/waves.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/points.hpp"
#include "expr/intervals/interval.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

[[noreturn]] void refuse(std::string object, std::string reason)
{
    throw Error(ErrorKind::InvalidArgument, std::move(object), std::move(reason));
}

Offset n_children(const Graph &graph, Offset node) noexcept
{
    return to_offset(graph.children(node).size());
}

std::vector<Offset> starts_of(const Graph &graph)
{
    std::vector<Offset> starts(to_size(graph.size()) + 1, 0);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        starts[to_size(node) + 1] = starts[to_size(node)] + n_children(graph, node);
    }
    return starts;
}

Curvature unary(const Profile &profile, std::span<Argument> arguments) noexcept
{
    arguments[0] = profile.argument;
    return profile.curvature;
}

PiecewiseLinear pwl_of(const Graph &graph, Offset node) noexcept
{
    return graph.points().pwl(graph.pwl(node));
}

Curvature outer_of(const Graph &graph,
                   const Operands &operands,
                   Offset node,
                   std::span<Argument> arguments) noexcept
{
    switch (graph.op(node))
    {
    case Operator::Constant:
    case Operator::Variable:
        return Curvature::Affine;
    case Operator::Sum:
        return sum_curvature(graph.coefficients(node), arguments);
    case Operator::Product:
        return product_curvature(graph.constant(node), operands, arguments);
    case Operator::Negation:
        return unary(bend(kNegation, operands[0]), arguments);
    case Operator::Division:
        return quotient_curvature(operands[0], operands[1], arguments);
    case Operator::ConstantPower:
        return unary(power_bend(operands[0], graph.constant(node)), arguments);
    case Operator::Power:
        return power_curvature(operands[0], operands[1], arguments);
    case Operator::Exp:
        return unary(bend(kExp, operands[0]), arguments);
    case Operator::Log:
        return unary(bend(kLog, operands[0]), arguments);
    case Operator::Sqrt:
        return unary(bend(kSqrt, operands[0]), arguments);
    case Operator::Sin:
        return unary(wave(kSine, operands[0]), arguments);
    case Operator::Cos:
        return unary(wave(kCosine, operands[0]), arguments);
    case Operator::Tan:
        return unary(wave(kTangent, operands[0]), arguments);
    case Operator::Abs:
        return unary(bend(kAbs, operands[0]), arguments);
    case Operator::Max:
        return largest_curvature(operands, arguments);
    case Operator::Min:
        return smallest_curvature(operands, arguments);
    case Operator::Asin:
        return unary(bend(kAsin, operands[0]), arguments);
    case Operator::Acos:
        return unary(bend(kAcos, operands[0]), arguments);
    case Operator::Atan:
        return unary(bend(kAtan, operands[0]), arguments);
    case Operator::Sinh:
        return unary(bend(kSinh, operands[0]), arguments);
    case Operator::Cosh:
        return unary(bend(kCosh, operands[0]), arguments);
    case Operator::Tanh:
        return unary(bend(kTanh, operands[0]), arguments);
    case Operator::Asinh:
        return unary(bend(kAsinh, operands[0]), arguments);
    case Operator::Acosh:
        return unary(bend(kAcosh, operands[0]), arguments);
    case Operator::Atanh:
        return unary(bend(kAtanh, operands[0]), arguments);
    case Operator::Entropy:
        return unary(bend(kEntropy, operands[0]), arguments);
    case Operator::SignPower:
        return unary(sign_bend(operands[0], graph.constant(node)), arguments);
    case Operator::Erf:
        return unary(bend(kErf, operands[0]), arguments);
    case Operator::PiecewiseLinear:
        return unary(segments(pwl_of(graph, node), operands[0]), arguments);
    }
    return Curvature::Unknown;
}

bool fits(Curvature child, const Argument &argument) noexcept
{
    const bool below = !argument.cut_below || is_concave(child);
    return below && (!argument.cut_above || is_convex(child));
}

bool keeps_convex(Curvature child, Monotonicity monotonicity) noexcept
{
    const bool rising = rises(monotonicity) && is_convex(child);
    return child == Curvature::Affine || rising || (falls(monotonicity) && is_concave(child));
}

bool keeps_concave(Curvature child, Monotonicity monotonicity) noexcept
{
    const bool rising = rises(monotonicity) && is_concave(child);
    return child == Curvature::Affine || rising || (falls(monotonicity) && is_convex(child));
}

}

Shapes::Shapes(const Graph &graph)
    : starts_(starts_of(graph)), curvatures_(to_size(graph.size()), Curvature::Unknown),
      arguments_(to_size(starts_.back()))
{
}

void Shapes::classify(const Graph &graph, std::span<const Interval> nodes)
{
    check(graph, nodes);
    for (Offset node = 0; node < graph.size(); ++node)
    {
        curvatures_[to_size(node)] = shape_of(graph, nodes, node);
    }
}

Curvature Shapes::curvature(Offset node) const noexcept
{
    return curvatures_[to_size(node)];
}

Monotonicity Shapes::monotonicity(Offset node, std::size_t position) const noexcept
{
    return arguments_[to_size(starts_[to_size(node)]) + position].monotonicity;
}

void Shapes::check(const Graph &graph, std::span<const Interval> nodes) const
{
    const auto held = to_offset(curvatures_.size());
    if (held != graph.size())
    {
        refuse("graph", "size " + text_of(graph.size()) + ", not " + text_of(held));
    }
    for (Offset node = 0; node < graph.size(); ++node)
    {
        const Offset n = n_children(graph, node);
        const Offset kept = starts_[to_size(node) + 1] - starts_[to_size(node)];
        if (n != kept)
        {
            const std::string children = n_text(n, "child", "children");
            refuse("graph",
                   "node " + text_of(node) + " has " + children + ", not " + text_of(kept));
        }
    }
    const auto entries = to_offset(nodes.size());
    if (entries != graph.size())
    {
        const std::string n = text_of(graph.size());
        refuse("nodes", "size " + text_of(entries) + ", not the node count " + n);
    }
}

std::span<Argument> Shapes::arguments_of(Offset node) noexcept
{
    const Offset start = starts_[to_size(node)];
    const Offset n = starts_[to_size(node) + 1] - start;
    return std::span(arguments_).subspan(to_size(start), to_size(n));
}

Curvature Shapes::shape_of(const Graph &graph,
                           std::span<const Interval> nodes,
                           Offset node) noexcept
{
    const std::span<Argument> arguments = arguments_of(node);
    const Operands operands{.children = graph.children(node), .nodes = nodes};
    if (nodes[to_size(node)].is_empty() || any_empty(operands))
    {
        std::ranges::fill(arguments, Argument{});
        return Curvature::Unknown;
    }
    const Curvature outer = outer_of(graph, operands, node, arguments);
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        if (is_point(operands[k]))
        {
            arguments[k].monotonicity = Monotonicity::Constant;
        }
    }
    return composed(outer, operands.children, arguments);
}

Curvature Shapes::composed(Curvature outer,
                           std::span<const Offset> children,
                           std::span<const Argument> arguments) const noexcept
{
    bool convex = is_convex(outer);
    bool concave = is_concave(outer);
    for (std::size_t k = 0; k < children.size(); ++k)
    {
        const Curvature child = curvatures_[to_size(children[k])];
        const bool kept = fits(child, arguments[k]);
        convex = convex && kept && keeps_convex(child, arguments[k].monotonicity);
        concave = concave && kept && keeps_concave(child, arguments[k].monotonicity);
    }
    return curvature_of(convex, concave);
}

}
