#include "io/nl/writer/printer.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "core/text/text.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/points.hpp"
#include "io/nl/operators.hpp"
#include "io/nl/writer/defined.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <numbers>
#include <ostream>
#include <span>

namespace biyos::nl
{

namespace
{

constexpr std::size_t kPair = 2;

}

Printer::Printer(const Model &model,
                 const Defined &defined,
                 std::span<const Index> position,
                 std::ostream &output)
    : model_(model), graph_(model.graph()), defined_(defined), position_(position), output_(output)
{
}

void Printer::print_top(Index function)
{
    const auto terms = model_.quadratic_terms(function, function + 1);
    const Offset root = model_.roots(function, function + 1).front();
    const bool unwrapped = root != kNoRoot && graph_.op(root) == Operator::Sum &&
                           identical(graph_.constant(root), 0.0) && !defined_.has(root);
    const std::size_t items = root == kNoRoot ? 0 : unwrapped ? graph_.children(root).size() : 1;
    open(1 + terms.size() + items);
    add_number(model_.constant(function));
    for (const QuadraticTerm &term : terms)
    {
        if (!identical(term.coefficient, 1.0))
        {
            add(Piece::Opcode, kTimesCode);
            add_number(term.coefficient);
        }
        add(Piece::Opcode, kTimesCode);
        add(Piece::Variable, term.row);
        add(Piece::Variable, term.column);
    }
    if (unwrapped)
    {
        const auto weights = graph_.coefficients(root);
        const auto children = graph_.children(root);
        for (std::size_t k = 0; k < children.size(); ++k)
        {
            add_scaled(children[k], weights[k]);
        }
    }
    else if (root != kNoRoot)
    {
        add(Piece::Node, root);
    }
    run();
}

void Printer::print_definition(const Definition &definition)
{
    const Offset node = definition.node;
    const Index lead = lead_of(graph_, node);
    output_ << 'V' << model_.n_variables() + defined_.index_of(node) << ' ' << lead << ' '
            << defined_.link_of(definition) << '\n';
    if (lead == 0)
    {
        expand(node);
        run();
        return;
    }
    const auto children = graph_.children(node);
    const auto weights = graph_.coefficients(node);
    for (std::size_t k = 0; k < to_size(lead); ++k)
    {
        output_ << position_[to_size(graph_.variable(children[k]))] << ' ' << text_of(weights[k])
                << '\n';
    }
    if (to_size(lead) == children.size())
    {
        add_number(graph_.constant(node));
    }
    else
    {
        add(Piece::Node, children.back());
    }
    run();
}

void Printer::run()
{
    stack_.assign(pieces_.rbegin(), pieces_.rend());
    pieces_.clear();
    while (!stack_.empty())
    {
        const Item item = stack_.back();
        stack_.pop_back();
        emit(item);
    }
}

void Printer::emit(const Item &item)
{
    switch (item.piece)
    {
    case Piece::Opcode:
        output_ << 'o' << item.node << '\n';
        return;
    case Piece::Count:
        output_ << item.node << '\n';
        return;
    case Piece::Number:
        output_ << 'n' << text_of(item.number) << '\n';
        return;
    case Piece::Variable:
        output_ << 'v' << position_[to_size(item.node)] << '\n';
        return;
    case Piece::Node:
        break;
    }
    if (graph_.op(item.node) == Operator::Variable)
    {
        output_ << 'v' << position_[to_size(graph_.variable(item.node))] << '\n';
        return;
    }
    if (defined_.has(item.node))
    {
        output_ << 'v' << model_.n_variables() + defined_.index_of(item.node) << '\n';
        return;
    }
    expand(item.node);
    stack_.insert(stack_.end(), pieces_.rbegin(), pieces_.rend());
    pieces_.clear();
}

void Printer::add(Piece piece, Offset node)
{
    pieces_.push_back({.piece = piece, .node = node, .number = 0.0});
}

void Printer::add_number(Real number)
{
    pieces_.push_back({.piece = Piece::Number, .node = 0, .number = number});
}

void Printer::add_scaled(Offset node, Real weight)
{
    if (!identical(weight, 1.0))
    {
        add(Piece::Opcode, kTimesCode);
        add_number(weight);
    }
    add(Piece::Node, node);
}

void Printer::open(std::size_t count)
{
    if (count == kPair)
    {
        add(Piece::Opcode, kPlusCode);
    }
    if (count > kPair)
    {
        add(Piece::Opcode, kSumCode);
        add(Piece::Count, to_offset(count));
    }
}

void Printer::expand(Offset node)
{
    const Operator op = graph_.op(node);
    const auto children = graph_.children(node);
    switch (op)
    {
    case Operator::Constant:
        add_number(graph_.constant(node));
        return;
    case Operator::Variable:
        add(Piece::Variable, graph_.variable(node));
        return;
    case Operator::Sum:
        expand_sum(node);
        return;
    case Operator::Product:
        expand_product(node);
        return;
    case Operator::ConstantPower:
        add(Piece::Opcode, kPowerCode);
        add(Piece::Node, children.front());
        add_number(graph_.constant(node));
        return;
    case Operator::Power:
        add(Piece::Opcode, kPowerCode);
        break;
    case Operator::Max:
    case Operator::Min:
        add(Piece::Opcode, code_of(op));
        add(Piece::Count, to_offset(children.size()));
        break;
    case Operator::PiecewiseLinear:
        expand_pieces(node);
        return;
    case Operator::Negation:
    case Operator::Division:
    case Operator::Exp:
    case Operator::Log:
    case Operator::Sqrt:
    case Operator::Sin:
    case Operator::Cos:
    case Operator::Tan:
    case Operator::Abs:
    case Operator::Asin:
    case Operator::Acos:
    case Operator::Atan:
    case Operator::Sinh:
    case Operator::Cosh:
    case Operator::Tanh:
    case Operator::Asinh:
    case Operator::Acosh:
    case Operator::Atanh:
        add(Piece::Opcode, code_of(op));
        break;
    case Operator::Entropy:
    case Operator::SignPower:
    case Operator::Erf:
    case Operator::Atan2:
        throw Error(ErrorKind::Internal, "io", "NL holds no such operator");
    }
    for (const Offset child : children)
    {
        add(Piece::Node, child);
    }
}

void Printer::expand_sum(Offset node)
{
    const auto children = graph_.children(node);
    const auto weights = graph_.coefficients(node);
    if (direct(graph_, node))
    {
        const bool minus = children.size() == kPair && identical(weights.back(), -1.0);
        if (minus)
        {
            add(Piece::Opcode, kMinusCode);
        }
        else
        {
            open(children.size());
        }
        for (const Offset child : children)
        {
            add(Piece::Node, child);
        }
        return;
    }
    const Real constant = graph_.constant(node);
    const bool offset = !identical(constant, 0.0);
    if (!offset && children.empty())
    {
        add_number(constant);
        return;
    }
    open((offset ? 1 : 0) + children.size());
    if (offset)
    {
        add_number(constant);
    }
    for (std::size_t k = 0; k < children.size(); ++k)
    {
        add_scaled(children[k], weights[k]);
    }
}

void Printer::expand_product(Offset node)
{
    const auto children = graph_.children(node);
    const Real coefficient = graph_.constant(node);
    const bool log10 = identical(coefficient, std::numbers::log10e) && children.size() == 1 &&
                       graph_.op(children.front()) == Operator::Log &&
                       !defined_.has(children.front());
    if (log10)
    {
        add(Piece::Opcode, kLog10Code);
        add(Piece::Node, graph_.children(children.front()).front());
        return;
    }
    const bool scaled = !identical(coefficient, 1.0);
    if (!scaled && children.empty())
    {
        add_number(coefficient);
        return;
    }
    if (scaled)
    {
        add(Piece::Opcode, kTimesCode);
        add_number(coefficient);
    }
    for (std::size_t k = 0; k < children.size(); ++k)
    {
        if (k + 1 < children.size())
        {
            add(Piece::Opcode, kTimesCode);
        }
        add(Piece::Node, children[k]);
    }
}

void Printer::expand_pieces(Offset node)
{
    const PiecewiseLinear pwl = graph_.points().piecewise_linear(graph_.piecewise_linear(node));
    const Real start = value_at(pwl, 0.0);
    if (!identical(start, 0.0))
    {
        add(Piece::Opcode, kPlusCode);
        add_number(start);
    }
    const std::size_t slopes = pwl.x.size() - 1;
    add(Piece::Opcode, kPiecesCode);
    add(Piece::Count, to_offset(slopes));
    for (std::size_t k = 0; k < slopes; ++k)
    {
        add_number(slope_of(pwl, k));
        if (k + 1 < slopes)
        {
            add_number(pwl.x[k + 1]);
        }
    }
    add(Piece::Node, graph_.children(node).front());
}

}
