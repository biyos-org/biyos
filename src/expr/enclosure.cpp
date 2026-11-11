#include "expr/enclosure.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "expr/elementary.hpp"
#include "expr/graph.hpp"
#include "expr/interval.hpp"
#include "expr/node.hpp"
#include "expr/periodic.hpp"
#include "expr/powers.hpp"

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
    std::span<const Interval> nodes;
};

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

[[noreturn]] void refuse(std::string object, std::string reason)
{
    throw Error(ErrorKind::InvalidArgument, std::move(object), std::move(reason));
}

void check_pass(const Graph &graph, std::span<const Interval> box, std::span<const Interval> nodes)
{
    const auto entries = static_cast<Offset>(nodes.size());
    if (entries != graph.size())
    {
        const std::string count = text_of(graph.size());
        refuse("nodes", "size " + text_of(entries) + ", not the node count " + count);
    }
    const auto covered = static_cast<Offset>(box.size());
    for (Offset node = 0; node < graph.size(); ++node)
    {
        if (graph.op(node) == Operator::Variable && graph.variable(node) >= covered)
        {
            const std::string variable = text_of(graph.variable(node));
            refuse("box", "size " + text_of(covered) + " does not cover variable " + variable);
        }
    }
}

Interval point(double value) noexcept
{
    return {value, value};
}

Interval first(const Pass &pass, Offset node) noexcept
{
    return pass.nodes[at(pass.graph.children(node)[0])];
}

Interval second(const Pass &pass, Offset node) noexcept
{
    return pass.nodes[at(pass.graph.children(node)[1])];
}

Interval variable_of(const Pass &pass, Offset node) noexcept
{
    return pass.box[static_cast<std::size_t>(pass.graph.variable(node))];
}

Interval sum_of(const Pass &pass, Offset node) noexcept
{
    const std::span<const Offset> terms = pass.graph.children(node);
    const std::span<const double> weights = pass.graph.coefficients(node);
    Interval total = point(pass.graph.constant(node));
    for (std::size_t k = 0; k < terms.size(); ++k)
    {
        total = total + point(weights[k]) * pass.nodes[at(terms[k])];
    }
    return total;
}

Interval product_of(const Pass &pass, Offset node) noexcept
{
    Interval total = point(pass.graph.constant(node));
    for (const Offset factor : pass.graph.children(node))
    {
        total = total * pass.nodes[at(factor)];
    }
    return total;
}

Interval largest(const Pass &pass, Offset node) noexcept
{
    const std::span<const Offset> operands = pass.graph.children(node);
    Interval best = pass.nodes[at(operands[0])];
    for (const Offset operand : operands.subspan(1))
    {
        best = maximum(best, pass.nodes[at(operand)]);
    }
    return best;
}

Interval smallest(const Pass &pass, Offset node) noexcept
{
    const std::span<const Offset> operands = pass.graph.children(node);
    Interval best = pass.nodes[at(operands[0])];
    for (const Offset operand : operands.subspan(1))
    {
        best = minimum(best, pass.nodes[at(operand)]);
    }
    return best;
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
    }
    return Interval::entire();
}

}

void enclose(const Graph &graph, std::span<const Interval> box, std::span<Interval> nodes)
{
    check_pass(graph, box, nodes);
    const Pass pass{.graph = graph, .box = box, .nodes = nodes};
    for (Offset node = 0; node < graph.size(); ++node)
    {
        nodes[at(node)] = enclosure(pass, node);
    }
}

}
