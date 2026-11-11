#include "expr/graph.hpp"

#include "core/index.hpp"
#include "expr/node.hpp"
#include "expr/room.hpp"

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
    return static_cast<Offset>(ops_.size());
}

Operator Graph::op(Offset node) const noexcept
{
    return ops_[at(node)];
}

std::span<const Offset> Graph::children(Offset node) const noexcept
{
    const Offset first = starts_[at(node)];
    const Offset count = starts_[at(node) + 1] - first;
    return std::span(children_).subspan(at(first), at(count));
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
    const std::size_t first = payloads_[at(node)] + 1;
    return std::span(coefficients_).subspan(first, children(node).size());
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
    make_room(coefficients_, coefficients_.size() + coefficients.size() + 1);
    const Offset node = append(Operator::Sum, coefficients_.size(), children);
    coefficients_.push_back(constant);
    coefficients_.insert(coefficients_.end(), coefficients.begin(), coefficients.end());
    return node;
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
    make_room(ops_, ops_.size() + 1);
    make_room(payloads_, payloads_.size() + 1);
    make_room(starts_, starts_.size() + 1);
    make_room(children_, children_.size() + children.size());
    const Offset node = size();
    ops_.push_back(op);
    payloads_.push_back(payload);
    children_.insert(children_.end(), children.begin(), children.end());
    starts_.push_back(static_cast<Offset>(children_.size()));
    return node;
}

}
