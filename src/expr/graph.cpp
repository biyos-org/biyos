#include "expr/graph.hpp"

#include "core/index.hpp"
#include "core/memory/room.hpp"
#include "expr/node.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

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

Offset Graph::size() const noexcept
{
    return static_cast<Offset>(operators_.size());
}

Operator Graph::op(Offset node) const noexcept
{
    return operators_[at(node)];
}

std::span<const Offset> Graph::children(Offset node) const noexcept
{
    const Offset start = starts_[at(node)];
    const Offset n = starts_[at(node) + 1] - start;
    return std::span(children_).subspan(at(start), at(n));
}

double Graph::constant(Offset node) const noexcept
{
    const std::uint64_t payload = payloads_[at(node)];
    if (op(node) == Operator::Sum)
    {
        return coefficients_[payload];
    }
    return std::bit_cast<double>(payload);
}

Index Graph::variable(Offset node) const noexcept
{
    return static_cast<Index>(payloads_[at(node)]);
}

std::span<const double> Graph::coefficients(Offset node) const noexcept
{
    if (op(node) != Operator::Sum)
    {
        return {};
    }
    const std::size_t start = payloads_[at(node)] + 1;
    return std::span(coefficients_).subspan(start, children(node).size());
}

Offset Graph::node_of(Index variable) const noexcept
{
    const auto index = static_cast<std::size_t>(variable);
    return index < variable_nodes_.size() ? variable_nodes_[index] : kNone;
}

Offset Graph::add(Operator op, std::span<const Offset> children)
{
    check(op, Payload::None, children);
    return append(op, 0, children);
}

Offset Graph::add(Operator op, double constant, std::span<const Offset> children)
{
    check(op, Payload::Constant, children);
    check_constant(op, constant);
    return append(op, std::bit_cast<std::uint64_t>(constant), children);
}

Offset Graph::add_variable(Index variable)
{
    check_variable(variable);
    const auto index = static_cast<std::size_t>(variable);
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

Offset Graph::add_sum(double constant,
                      std::span<const Offset> children,
                      std::span<const double> coefficients)
{
    if (inside(coefficients, coefficients_))
    {
        const std::vector<double> copy(coefficients.begin(), coefficients.end());
        return add_sum(constant, children, copy);
    }
    check(Operator::Sum, Payload::Coefficients, children);
    check_constant(constant);
    check_coefficients(coefficients, static_cast<Offset>(children.size()));
    reserve_more(coefficients_, coefficients.size() + 1);
    const Offset node = append(Operator::Sum, coefficients_.size(), children);
    coefficients_.push_back(constant);
    coefficients_.insert(coefficients_.end(), coefficients.begin(), coefficients.end());
    return node;
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
            payloads_[at(node)] = static_cast<std::uint64_t>(map[index]);
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
    };
}

void Graph::truncate(const Mark &mark) noexcept
{
    for (Offset node = mark.nodes; node < size(); ++node)
    {
        if (op(node) == Operator::Variable)
        {
            variable_nodes_[at(variable(node))] = kNone;
        }
    }
    const std::size_t nodes = at(mark.nodes);
    operators_.resize(nodes);
    payloads_.resize(nodes);
    children_.resize(at(starts_[nodes]));
    starts_.resize(nodes + 1);
    coefficients_.resize(mark.coefficients);
    variable_nodes_.resize(mark.variables);
}

void Graph::check(Operator op, Payload payload, std::span<const Offset> children) const
{
    check_shape(op, payload, static_cast<Offset>(children.size()));
    for (const Offset child : children)
    {
        check_child(child, size());
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
    starts_.push_back(static_cast<Offset>(children_.size()));
    return node;
}

}
