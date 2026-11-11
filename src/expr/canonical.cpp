#include "expr/canonical.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "expr/parents.hpp"
#include "expr/terms.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

constexpr double kSquare = 2.0;
constexpr double kUnit = 1.0;
constexpr std::size_t kDegree = 2;
constexpr Offset kNoFactor = -1;

struct Item
{
    Offset node;
    double factor;
    bool root;
};

struct Monomial
{
    double coefficient;
    std::array<Index, kDegree> variables;
    std::size_t degree;
    Offset compound;
    std::size_t compound_count;
};

void check_parents(const Parents &parents, Offset node)
{
    if (node >= parents.size())
    {
        const std::string reason = "parents are older than node " + text_of(node);
        throw Error(ErrorKind::InvalidArgument, "expression", reason);
    }
}

bool same(double a, double b) noexcept
{
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

bool is_zero(double value) noexcept
{
    return std::fpclassify(value) == FP_ZERO;
}

bool is_finite(double value) noexcept
{
    return std::isfinite(value);
}

bool scales(const Graph &graph, Offset sum, double factor) noexcept
{
    if (!is_finite(factor * graph.constant(sum)))
    {
        return false;
    }
    for (const double weight : graph.coefficients(sum))
    {
        if (!is_finite(factor * weight))
        {
            return false;
        }
    }
    return true;
}

void add_factor(const Graph &graph, Offset factor, Monomial &monomial)
{
    const Operator op = graph.op(factor);
    if (op == Operator::Constant)
    {
        monomial.coefficient *= graph.constant(factor);
        return;
    }
    if (op != Operator::Variable)
    {
        monomial.compound = factor;
        ++monomial.compound_count;
        return;
    }
    if (monomial.degree < kDegree)
    {
        monomial.variables[monomial.degree] = graph.variable(factor);
    }
    ++monomial.degree;
}

Monomial monomial_of(const Graph &graph, Offset node, double factor)
{
    Monomial monomial{
        .coefficient = factor * graph.constant(node),
        .variables = {},
        .degree = 0,
        .compound = kNoFactor,
        .compound_count = 0,
    };
    for (const Offset child : graph.children(node))
    {
        add_factor(graph, child, monomial);
    }
    return monomial;
}

Parts whole(Offset node)
{
    return {.constant = 0.0, .linear = {}, .quadratic = {}, .remainder = node};
}

class Walk
{
  public:
    Walk(const Graph &graph, const Parents &parents);

    void run(Offset node);
    bool moved() const noexcept;
    bool finite() const noexcept;
    Parts parts(Graph &graph);

  private:
    void visit(const Item &item);
    void open_constant(const Item &item);
    void open_sum(const Item &item);
    void open_negation(const Item &item);
    void open_product(const Item &item);
    void open_power(const Item &item);
    bool shared(const Item &item) const noexcept;
    void push(Offset node, double factor);
    void add_monomial(const Monomial &monomial);
    void add_constant(double value);
    void add_linear(Index variable, double coefficient);
    void add_quadratic(Index first, Index second, double coefficient);
    void keep(const Item &item);
    void close(const Item &item);
    Offset remainder(Graph &graph) const;

    const Graph &graph_;
    const Parents &parents_;
    std::vector<Item> stack_;
    double constant_ = 0.0;
    std::vector<LinearTerm> linear_;
    std::vector<QuadraticTerm> quadratic_;
    std::vector<Offset> kept_;
    std::vector<double> weights_;
    bool moved_ = false;
    bool closed_ = false;
};

Walk::Walk(const Graph &graph, const Parents &parents) : graph_(graph), parents_(parents) {}

void Walk::run(Offset node)
{
    stack_.push_back({.node = node, .factor = kUnit, .root = true});
    while (!stack_.empty())
    {
        const Item item = stack_.back();
        stack_.pop_back();
        visit(item);
    }
    add_alike(linear_);
    add_alike(quadratic_);
}

bool Walk::moved() const noexcept
{
    return moved_;
}

bool Walk::finite() const noexcept
{
    return is_finite(constant_) &&
           std::ranges::all_of(linear_, is_finite, &LinearTerm::coefficient) &&
           std::ranges::all_of(quadratic_, is_finite, &QuadraticTerm::coefficient);
}

Parts Walk::parts(Graph &graph)
{
    return {
        .constant = constant_,
        .linear = std::move(linear_),
        .quadratic = std::move(quadratic_),
        .remainder = remainder(graph),
    };
}

void Walk::visit(const Item &item)
{
    switch (graph_.op(item.node))
    {
    case Operator::Constant:
        open_constant(item);
        break;
    case Operator::Variable:
        add_linear(graph_.variable(item.node), item.factor);
        break;
    case Operator::Sum:
        open_sum(item);
        break;
    case Operator::Product:
        open_product(item);
        break;
    case Operator::Negation:
        open_negation(item);
        break;
    case Operator::ConstantPower:
        open_power(item);
        break;
    case Operator::Division:
    case Operator::Power:
    case Operator::Exp:
    case Operator::Log:
    case Operator::Sqrt:
    case Operator::Sin:
    case Operator::Cos:
    case Operator::Tan:
    case Operator::Abs:
    case Operator::Max:
    case Operator::Min:
    case Operator::Asin:
    case Operator::Acos:
    case Operator::Atan:
    case Operator::Sinh:
    case Operator::Cosh:
    case Operator::Tanh:
    case Operator::Asinh:
    case Operator::Acosh:
    case Operator::Atanh:
    case Operator::Entropy:
    case Operator::SignPower:
    case Operator::Erf:
        keep(item);
        break;
    }
}

void Walk::open_constant(const Item &item)
{
    const double value = item.factor * graph_.constant(item.node);
    if (!is_finite(value))
    {
        close(item);
        return;
    }
    add_constant(value);
}

void Walk::open_sum(const Item &item)
{
    if (shared(item) || !scales(graph_, item.node, item.factor))
    {
        close(item);
        return;
    }
    const double constant = graph_.constant(item.node);
    if (!is_zero(constant))
    {
        add_constant(item.factor * constant);
    }
    const auto children = graph_.children(item.node);
    const auto weights = graph_.coefficients(item.node);
    for (std::size_t k = children.size(); k > 0; --k)
    {
        push(children[k - 1], item.factor * weights[k - 1]);
    }
}

void Walk::open_negation(const Item &item)
{
    if (shared(item))
    {
        close(item);
        return;
    }
    push(graph_.children(item.node).front(), -item.factor);
}

void Walk::open_product(const Item &item)
{
    const Monomial monomial = monomial_of(graph_, item.node, item.factor);
    if (!is_finite(monomial.coefficient))
    {
        close(item);
        return;
    }
    if (monomial.compound_count == 0 && monomial.degree <= kDegree)
    {
        add_monomial(monomial);
        return;
    }
    if (monomial.compound_count != 1 || monomial.degree != 0)
    {
        keep(item);
        return;
    }
    if (shared(item))
    {
        close(item);
        return;
    }
    push(monomial.compound, monomial.coefficient);
}

void Walk::open_power(const Item &item)
{
    const Offset base = graph_.children(item.node).front();
    if (!same(graph_.constant(item.node), kSquare) || graph_.op(base) != Operator::Variable)
    {
        keep(item);
        return;
    }
    const Index variable = graph_.variable(base);
    add_quadratic(variable, variable, item.factor);
}

bool Walk::shared(const Item &item) const noexcept
{
    return !item.root && parents_.of(item.node).size() > 1;
}

void Walk::push(Offset node, double factor)
{
    stack_.push_back({.node = node, .factor = factor, .root = false});
}

void Walk::add_monomial(const Monomial &monomial)
{
    if (monomial.degree == 0)
    {
        add_constant(monomial.coefficient);
        return;
    }
    if (monomial.degree == 1)
    {
        add_linear(monomial.variables[0], monomial.coefficient);
        return;
    }
    add_quadratic(monomial.variables[0], monomial.variables[1], monomial.coefficient);
}

void Walk::add_constant(double value)
{
    constant_ += value;
    moved_ = true;
}

void Walk::add_linear(Index variable, double coefficient)
{
    linear_.push_back({.variable = variable, .coefficient = coefficient});
    moved_ = true;
}

void Walk::add_quadratic(Index first, Index second, double coefficient)
{
    quadratic_.push_back({.first = first, .second = second, .coefficient = coefficient});
    moved_ = true;
}

void Walk::keep(const Item &item)
{
    kept_.push_back(item.node);
    weights_.push_back(item.factor);
}

void Walk::close(const Item &item)
{
    keep(item);
    closed_ = true;
}

Offset Walk::remainder(Graph &graph) const
{
    if (kept_.empty())
    {
        return kNoRemainder;
    }
    if (kept_.size() == 1 && !closed_ && same(weights_.front(), kUnit))
    {
        return kept_.front();
    }
    return graph.add_sum(0.0, kept_, weights_);
}

}

Parts split(Graph &graph, const Parents &parents, Offset node)
{
    check_child(node, graph.size());
    check_parents(parents, node);
    Walk walk(graph, parents);
    walk.run(node);
    if (!walk.moved() || !walk.finite())
    {
        return whole(node);
    }
    return walk.parts(graph);
}

}
