#include "expr/builder.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "expr/room.hpp"

#include <cmath>
#include <cstddef>
#include <span>
#include <string>

namespace biyos
{

namespace
{

constexpr Offset kPending = -1;
constexpr double kFactor = 1.0;

[[noreturn]] void refuse(const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, "expression", reason);
}

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

bool product_fits(double left, double right) noexcept
{
    const bool zero = std::fpclassify(left) == FP_ZERO || std::fpclassify(right) == FP_ZERO;
    return zero || std::isnormal(left * right);
}

std::string operands_text(Offset n)
{
    return n_text(n, "operand", "operands");
}

}

Builder::Builder(Graph &graph) : graph_(graph) {}

void Builder::constant(double value)
{
    push(graph_.add(Operator::Constant, value, {}));
}

void Builder::var(Index var)
{
    push(graph_.add_var(var));
}

void Builder::node(Offset id)
{
    check_child(id, graph_.size());
    push(id);
}

void Builder::sum(double constant, std::span<const double> coefficients)
{
    const auto n = static_cast<Offset>(coefficients.size());
    check_shape(Operator::Sum, Payload::Coefficients, n);
    check_constant(constant);
    check_coefficients(coefficients, n);
    const std::size_t base = take(n);
    double merged = constant;
    next_terms_.clear();
    next_weights_.clear();
    for (std::size_t k = 0; k < coefficients.size(); ++k)
    {
        add_term(base + k, coefficients[k], merged);
    }
    replace(base, Operator::Sum, merged);
}

void Builder::product(double coefficient, Offset n)
{
    check_shape(Operator::Product, Payload::Constant, n);
    check_constant(coefficient);
    const std::size_t base = take(n);
    double merged = coefficient;
    next_terms_.clear();
    next_weights_.clear();
    for (std::size_t k = base; k < stack_.size(); ++k)
    {
        add_factor(k, merged);
    }
    replace(base, Operator::Product, merged);
}

void Builder::power(double exponent)
{
    raise(Operator::ConstantPower, exponent);
}

void Builder::sign_power(double exponent)
{
    raise(Operator::SignPower, exponent);
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

Offset Builder::finish()
{
    if (stack_.size() != 1)
    {
        refuse(operands_text(static_cast<Offset>(stack_.size())) + " left, not 1");
    }
    const Offset node = resolve(0);
    pop(0);
    return node;
}

void Builder::raise(Operator op, double exponent)
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
        .first = static_cast<Offset>(terms_.size()),
        .count = 0,
    });
}

std::size_t Builder::take(Offset n) const
{
    const auto given = static_cast<Offset>(stack_.size());
    if (n > given)
    {
        refuse(operands_text(n) + " taken, " + text_of(given) + " given");
    }
    return at(given - n);
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
    const auto terms = std::span(terms_).subspan(at(operand.first), at(operand.count));
    if (operand.op != Operator::Sum)
    {
        return graph_.add(operand.op, operand.constant, terms);
    }
    const auto weights = std::span(weights_).subspan(at(operand.first), at(operand.count));
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

bool Builder::merges(const Operand &operand, double coefficient, double constant) const
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
        if (!product_fits(coefficient, weights_[at(k)]))
        {
            return false;
        }
    }
    return true;
}

void Builder::add_term(std::size_t position, double coefficient, double &constant)
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
        next_terms_.push_back(terms_[at(k)]);
        next_weights_.push_back(coefficient * weights_[at(k)]);
    }
}

void Builder::add_factor(std::size_t position, double &coefficient)
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
        next_terms_.push_back(terms_[at(k)]);
        next_weights_.push_back(kFactor);
    }
}

void Builder::pop(std::size_t base)
{
    const std::size_t beg = at(stack_[base].first);
    stack_.resize(base);
    terms_.resize(beg);
    weights_.resize(beg);
}

void Builder::replace(std::size_t base, Operator op, double constant)
{
    const std::size_t needed = at(stack_[base].first) + next_terms_.size();
    make_room(terms_, needed);
    make_room(weights_, needed);
    pop(base);
    const auto beg = static_cast<Offset>(terms_.size());
    const auto n = static_cast<Offset>(next_terms_.size());
    terms_.insert(terms_.end(), next_terms_.begin(), next_terms_.end());
    weights_.insert(weights_.end(), next_weights_.begin(), next_weights_.end());
    stack_.push_back(Operand{
        .node = kPending,
        .op = op,
        .constant = constant,
        .first = beg,
        .count = n,
    });
}

}
