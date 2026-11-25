#include "expr/graph/builder.hpp"

#include "core/error.hpp"
#include "core/memory/room.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/points.hpp"

#include <cmath>
#include <cstddef>
#include <span>
#include <string>

namespace biyos
{

namespace
{

constexpr Offset kPending = -1;
constexpr Real kFactor = 1.0;

[[noreturn]] void refuse(const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, "expression", reason);
}

bool product_fits(Real left, Real right) noexcept
{
    const bool zero = std::fpclassify(left) == FP_ZERO || std::fpclassify(right) == FP_ZERO;
    return zero || std::isnormal(left * right);
}

std::string operand_text(Offset n)
{
    return n_text(n, "operand", "operands");
}

}

Builder::Builder(Graph &graph) : graph_(graph) {}

void Builder::constant(Real value)
{
    push(graph_.add(Operator::Constant, value, {}));
}

void Builder::variable(Index variable)
{
    push(graph_.add_variable(variable));
}

void Builder::node(Offset id)
{
    check_child(id, graph_.size());
    push(id);
}

void Builder::sum(Real constant, std::span<const Real> coefficients)
{
    const auto n = to_offset(coefficients.size());
    check_shape(Operator::Sum, Payload::Coefficients, n);
    check_constant(constant);
    check_coefficients(coefficients, n);
    const std::size_t base = take(n);
    Real merged = constant;
    next_terms_.clear();
    next_weights_.clear();
    for (std::size_t k = 0; k < coefficients.size(); ++k)
    {
        add_term(base + k, coefficients[k], merged);
    }
    replace(base, Operator::Sum, merged);
}

void Builder::product(Real coefficient, Offset n)
{
    check_shape(Operator::Product, Payload::Constant, n);
    check_constant(coefficient);
    const std::size_t base = take(n);
    Real merged = coefficient;
    next_terms_.clear();
    next_weights_.clear();
    for (std::size_t k = base; k < stack_.size(); ++k)
    {
        add_factor(k, merged);
    }
    replace(base, Operator::Product, merged);
}

void Builder::power(Real exponent)
{
    raise(Operator::ConstantPower, exponent);
}

void Builder::sign_power(Real exponent)
{
    raise(Operator::SignPower, exponent);
}

void Builder::piecewise(Offset pwl)
{
    check_piecewise_linear(pwl, graph_.points().size());
    const std::size_t base = take(1);
    collect(base);
    const Offset node = graph_.add_piecewise(pwl, next_terms_);
    pop(base);
    push(node);
}

void Builder::apply(Operator op, Offset n)
{
    check_shape(op, Payload::None, n);
    const std::size_t base = take(n);
    collect(base);
    const Offset node = graph_.add(op, next_terms_);
    pop(base);
    push(node);
}

Offset Builder::keep()
{
    return resolve(take(1));
}

Offset Builder::finish()
{
    if (stack_.size() != 1)
    {
        refuse(operand_text(to_offset(stack_.size())) + " left, not 1");
    }
    const Offset node = resolve(0);
    pop(0);
    return node;
}

void Builder::raise(Operator op, Real exponent)
{
    check_constant(op, exponent);
    const std::size_t base = take(1);
    collect(base);
    const Offset node = graph_.add(op, exponent, next_terms_);
    pop(base);
    push(node);
}

void Builder::push(Offset node)
{
    stack_.push_back(Operand{
        .node = node,
        .op = graph_.op(node),
        .constant = 0.0,
        .first = to_offset(terms_.size()),
        .count = 0,
    });
}

std::size_t Builder::take(Offset n) const
{
    const auto given = to_offset(stack_.size());
    if (n > given)
    {
        refuse(operand_text(n) + " taken, " + text_of(given) + " given");
    }
    return to_size(given - n);
}

Offset Builder::resolve(std::size_t position)
{
    Operand &operand = stack_[position];
    if (operand.node == kPending)
    {
        operand.node = append(operand);
    }
    return operand.node;
}

Offset Builder::append(const Operand &operand)
{
    const auto terms = std::span(terms_).subspan(to_size(operand.first), to_size(operand.count));
    if (operand.op != Operator::Sum)
    {
        return graph_.add(operand.op, operand.constant, terms);
    }
    const auto weights =
        std::span(weights_).subspan(to_size(operand.first), to_size(operand.count));
    return graph_.add_sum(operand.constant, terms, weights);
}

void Builder::collect(std::size_t base)
{
    next_terms_.clear();
    for (std::size_t k = base; k < stack_.size(); ++k)
    {
        next_terms_.push_back(resolve(k));
    }
}

bool Builder::merges(const Operand &operand, Real coefficient, Real constant) const
{
    if (operand.node != kPending || operand.op != Operator::Sum)
    {
        return false;
    }
    if (!product_fits(coefficient, operand.constant))
    {
        return false;
    }
    if (!std::isfinite(constant + coefficient * operand.constant))
    {
        return false;
    }
    for (Offset k = operand.first; k < operand.first + operand.count; ++k)
    {
        if (!product_fits(coefficient, weights_[to_size(k)]))
        {
            return false;
        }
    }
    return true;
}

void Builder::add_term(std::size_t position, Real coefficient, Real &constant)
{
    const Operand &operand = stack_[position];
    if (!merges(operand, coefficient, constant))
    {
        next_terms_.push_back(resolve(position));
        next_weights_.push_back(coefficient);
        return;
    }
    constant += coefficient * operand.constant;
    for (Offset k = operand.first; k < operand.first + operand.count; ++k)
    {
        next_terms_.push_back(terms_[to_size(k)]);
        next_weights_.push_back(coefficient * weights_[to_size(k)]);
    }
}

void Builder::add_factor(std::size_t position, Real &coefficient)
{
    const Operand &operand = stack_[position];
    const bool pending = operand.node == kPending && operand.op == Operator::Product;
    if (!pending || !product_fits(coefficient, operand.constant))
    {
        next_terms_.push_back(resolve(position));
        next_weights_.push_back(kFactor);
        return;
    }
    coefficient *= operand.constant;
    for (Offset k = operand.first; k < operand.first + operand.count; ++k)
    {
        next_terms_.push_back(terms_[to_size(k)]);
        next_weights_.push_back(kFactor);
    }
}

void Builder::pop(std::size_t base)
{
    const std::size_t start = to_size(stack_[base].first);
    stack_.resize(base);
    terms_.resize(start);
    weights_.resize(start);
}

void Builder::replace(std::size_t base, Operator op, Real constant)
{
    const std::size_t needed = to_size(stack_[base].first) + next_terms_.size();
    make_room(terms_, needed);
    make_room(weights_, needed);
    pop(base);
    const auto start = to_offset(terms_.size());
    const auto n = to_offset(next_terms_.size());
    terms_.insert(terms_.end(), next_terms_.begin(), next_terms_.end());
    weights_.insert(weights_.end(), next_weights_.begin(), next_weights_.end());
    stack_.push_back(Operand{
        .node = kPending,
        .op = op,
        .constant = constant,
        .first = start,
        .count = n,
    });
}

}
