#include "expr/graph/graph.hpp"

#include "core/memory/room.hpp"
#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/points.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

constexpr Offset kNone = -1;

template <typename T> bool inside(std::span<const T> values, const std::vector<T> &storage) noexcept
{
    const std::less<const T *> before;
    const T *first = storage.data();
    const T *last = first + storage.size();
    return !values.empty() && !before(values.data(), first) && before(values.data(), last);
}

}

Graph::Graph() : starts_(1, 0) {}

Graph::Graph(Points points) : starts_(1, 0), points_(std::move(points)) {}

Offset Graph::size() const noexcept
{
    return to_offset(operators_.size());
}

Operator Graph::op(Offset node) const noexcept
{
    return operators_[to_size(node)];
}

std::span<const Offset> Graph::children(Offset node) const noexcept
{
    return slice(std::span(children_), starts_, to_size(node));
}

Real Graph::constant(Offset node) const noexcept
{
    const std::uint64_t payload = payloads_[to_size(node)];
    if (op(node) == Operator::Sum)
    {
        return coefficients_[payload];
    }
    return std::bit_cast<Real>(payload);
}

Index Graph::variable(Offset node) const noexcept
{
    return static_cast<Index>(payloads_[to_size(node)]);
}

std::span<const Real> Graph::coefficients(Offset node) const noexcept
{
    if (op(node) != Operator::Sum)
    {
        return {};
    }
    const std::size_t start = payloads_[to_size(node)] + 1;
    return std::span(coefficients_).subspan(start, children(node).size());
}

Offset Graph::piecewise_linear(Offset node) const noexcept
{
    return static_cast<Offset>(payloads_[to_size(node)]);
}

Offset Graph::node_of(Index variable) const noexcept
{
    const auto index = to_size(variable);
    return index < variable_nodes_.size() ? variable_nodes_[index] : kNone;
}

const Points &Graph::points() const noexcept
{
    return points_;
}

Offset Graph::add(Operator op, std::span<const Offset> children)
{
    check(op, Payload::None, children);
    return append(op, 0, children);
}

Offset Graph::add(Operator op, Real constant, std::span<const Offset> children)
{
    check(op, Payload::Constant, children);
    check_constant(op, constant);
    return append(op, std::bit_cast<Bits>(constant), children);
}

Offset Graph::add_variable(Index variable)
{
    check_variable(variable);
    const auto index = to_size(variable);
    if (index >= variable_nodes_.size())
    {
        variable_nodes_.resize(index + 1, kNone);
    }
    if (variable_nodes_[index] == kNone)
    {
        const auto payload = static_cast<std::uint64_t>(variable);
        variable_nodes_[index] = append(Operator::Variable, payload, {});
    }
    return variable_nodes_[index];
}

Offset Graph::add_sum(Real constant,
                      std::span<const Offset> children,
                      std::span<const Real> coefficients)
{
    if (inside(coefficients, coefficients_))
    {
        const std::vector<Real> copy(coefficients.begin(), coefficients.end());
        return add_sum(constant, children, copy);
    }
    check(Operator::Sum, Payload::Coefficients, children);
    check_constant(constant);
    check_coefficients(coefficients, to_offset(children.size()));
    reserve_more(coefficients_, coefficients.size() + 1);
    const Offset node = append(Operator::Sum, coefficients_.size(), children);
    coefficients_.push_back(constant);
    coefficients_.insert(coefficients_.end(), coefficients.begin(), coefficients.end());
    return node;
}

Offset Graph::add_piecewise_linear(std::span<const Real> x, std::span<const Real> y)
{
    return points_.add(x, y);
}

Offset Graph::add_piecewise(Offset pwl, std::span<const Offset> children)
{
    check(Operator::PiecewiseLinear, Payload::Points, children);
    check_piecewise_linear(pwl, points_.size());
    return append(Operator::PiecewiseLinear, static_cast<std::uint64_t>(pwl), children);
}

void Graph::renumber_variables(std::span<const Index> map) noexcept
{
    std::size_t kept = 0;
    for (std::size_t index = 0; index < variable_nodes_.size(); ++index)
    {
        if (map[index] < 0)
        {
            continue;
        }
        const Offset node = variable_nodes_[index];
        if (node != kNone)
        {
            payloads_[to_size(node)] = static_cast<std::uint64_t>(map[index]);
        }
        variable_nodes_[kept] = node;
        ++kept;
    }
    variable_nodes_.resize(kept);
}

Graph::Mark Graph::mark() const noexcept
{
    return {
        .nodes = size(),
        .coefficients = coefficients_.size(),
        .variables = variable_nodes_.size(),
        .piecewise_linears = points_.size(),
    };
}

void Graph::truncate(const Mark &mark) noexcept
{
    for (Offset node = mark.nodes; node < size(); ++node)
    {
        if (op(node) == Operator::Variable)
        {
            variable_nodes_[to_size(variable(node))] = kNone;
        }
    }
    const std::size_t nodes = to_size(mark.nodes);
    operators_.resize(nodes);
    payloads_.resize(nodes);
    children_.resize(to_size(starts_[nodes]));
    starts_.resize(nodes + 1);
    coefficients_.resize(mark.coefficients);
    variable_nodes_.resize(mark.variables);
    points_.truncate(mark.piecewise_linears);
}

void Graph::check(Operator op, Payload payload, std::span<const Offset> children) const
{
    check_shape(op, payload, to_offset(children.size()));
    for (const Offset child : children)
    {
        check_child(child, size());
        check_operand(op, child, operators_[to_size(child)]);
    }
}

Offset Graph::append(Operator op, std::uint64_t payload, std::span<const Offset> children)
{
    if (inside(children, children_))
    {
        const std::vector<Offset> copy(children.begin(), children.end());
        return append(op, payload, copy);
    }
    reserve_more(operators_, 1);
    reserve_more(payloads_, 1);
    reserve_more(starts_, 1);
    reserve_more(children_, children.size());
    const Offset node = size();
    operators_.push_back(op);
    payloads_.push_back(payload);
    children_.insert(children_.end(), children.begin(), children.end());
    starts_.push_back(to_offset(children_.size()));
    return node;
}

}
