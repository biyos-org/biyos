#include "expr/analysis/intervals/enclosure.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/text/text.hpp"
#include "expr/analysis/intervals/elementary.hpp"
#include "expr/analysis/intervals/interval.hpp"
#include "expr/analysis/intervals/periodic.hpp"
#include "expr/analysis/intervals/piecewise.hpp"
#include "expr/analysis/intervals/powers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <utility>

namespace biyos
{

namespace
{

struct Pass
{
    const Graph &graph;
    std::span<const Interval> box;
    const Tolerances &tolerances;
    std::span<const Interval> nodes;
};

[[noreturn]] void refuse(std::string object, std::string reason)
{
    throw Error(ErrorKind::InvalidArgument, std::move(object), std::move(reason));
}

void check_pass(const Graph &graph, std::span<const Interval> box, std::span<const Interval> nodes)
{
    const auto entries = to_offset(nodes.size());
    if (entries != graph.size())
    {
        const std::string n = text_of(graph.size());
        refuse("nodes", "size " + text_of(entries) + ", not the node count " + n);
    }
    const auto covered = to_offset(box.size());
    for (Offset node = 0; node < graph.size(); ++node)
    {
        if (graph.op(node) == Operator::Variable && graph.variable(node) >= covered)
        {
            const std::string variable = text_of(graph.variable(node));
            refuse("box", "size " + text_of(covered) + " does not cover variable " + variable);
        }
    }
}

Interval point(Real value) noexcept
{
    return {value, value};
}

Interval first(const Pass &pass, Offset node) noexcept
{
    return pass.nodes[to_size(pass.graph.children(node)[0])];
}

Interval second(const Pass &pass, Offset node) noexcept
{
    return pass.nodes[to_size(pass.graph.children(node)[1])];
}

Interval variable_of(const Pass &pass, Offset node) noexcept
{
    return pass.box[to_size(pass.graph.variable(node))];
}

Interval sum_of(const Pass &pass, Offset node) noexcept
{
    const std::span<const Offset> terms = pass.graph.children(node);
    const std::span<const Real> weights = pass.graph.coefficients(node);
    Interval total = point(pass.graph.constant(node));
    for (std::size_t k = 0; k < terms.size(); ++k)
    {
        total = total + point(weights[k]) * pass.nodes[to_size(terms[k])];
    }
    return total;
}

Interval product_of(const Pass &pass, Offset node) noexcept
{
    Interval total = point(pass.graph.constant(node));
    for (const Offset factor : pass.graph.children(node))
    {
        total = total * pass.nodes[to_size(factor)];
    }
    return total;
}

Interval largest(const Pass &pass, Offset node) noexcept
{
    const std::span<const Offset> operands = pass.graph.children(node);
    Interval best = pass.nodes[to_size(operands[0])];
    for (const Offset operand : operands.subspan(1))
    {
        best = maximum(best, pass.nodes[to_size(operand)]);
    }
    return best;
}

Interval smallest(const Pass &pass, Offset node) noexcept
{
    const std::span<const Offset> operands = pass.graph.children(node);
    Interval best = pass.nodes[to_size(operands[0])];
    for (const Offset operand : operands.subspan(1))
    {
        best = minimum(best, pass.nodes[to_size(operand)]);
    }
    return best;
}

Interval piecewise_of(const Pass &pass, Offset node) noexcept
{
    return piecewise(pass.graph.points().piecewise_linear(pass.graph.piecewise_linear(node)),
                     first(pass, node));
}

Interval undecided(Interval a, Interval b) noexcept
{
    if (a.is_empty() || b.is_empty())
    {
        return Interval::empty();
    }
    return {0.0, 1.0};
}

Interval enclosure(const Pass &pass, Offset node) noexcept
{
    switch (pass.graph.op(node))
    {
    case Operator::Constant:
        return point(pass.graph.constant(node));
    case Operator::Variable:
        return variable_of(pass, node);
    case Operator::Sum:
        return sum_of(pass, node);
    case Operator::Product:
        return product_of(pass, node);
    case Operator::Negation:
        return -first(pass, node);
    case Operator::Division:
        return first(pass, node) / second(pass, node);
    case Operator::ConstantPower:
        return power(first(pass, node), point(pass.graph.constant(node)));
    case Operator::Power:
        return power(first(pass, node), second(pass, node));
    case Operator::Exp:
        return exp(first(pass, node));
    case Operator::Log:
        return log(first(pass, node));
    case Operator::Sqrt:
        return sqrt(first(pass, node));
    case Operator::Sin:
        return sin(first(pass, node));
    case Operator::Cos:
        return cos(first(pass, node));
    case Operator::Tan:
        return tan(first(pass, node));
    case Operator::Abs:
        return abs(first(pass, node));
    case Operator::Max:
        return largest(pass, node);
    case Operator::Min:
        return smallest(pass, node);
    case Operator::Asin:
        return asin(first(pass, node));
    case Operator::Acos:
        return acos(first(pass, node));
    case Operator::Atan:
        return atan(first(pass, node));
    case Operator::Sinh:
        return sinh(first(pass, node));
    case Operator::Cosh:
        return cosh(first(pass, node));
    case Operator::Tanh:
        return tanh(first(pass, node));
    case Operator::Asinh:
        return asinh(first(pass, node));
    case Operator::Acosh:
        return acosh(first(pass, node));
    case Operator::Atanh:
        return atanh(first(pass, node));
    case Operator::Entropy:
        return entropy(first(pass, node));
    case Operator::SignPower:
        return sign_power(first(pass, node), pass.graph.constant(node));
    case Operator::Erf:
        return erf(first(pass, node));
    case Operator::PiecewiseLinear:
        return piecewise_of(pass, node);
    case Operator::Atan2:
        return atan2(first(pass, node), second(pass, node));
    case Operator::LessEqual:
    case Operator::GreaterEqual:
    case Operator::Equal:
    case Operator::Less:
    case Operator::Greater:
    case Operator::NotEqual:
        return undecided(first(pass, node), second(pass, node));
    }
    return Interval::entire();
}

}

void enclose(const Graph &graph,
             std::span<const Interval> box,
             const Tolerances &tolerances,
             std::span<Interval> nodes)
{
    check_pass(graph, box, nodes);
    const Pass pass{.graph = graph, .box = box, .tolerances = tolerances, .nodes = nodes};
    for (Offset node = 0; node < graph.size(); ++node)
    {
        nodes[to_size(node)] = enclosure(pass, node);
    }
}

}
