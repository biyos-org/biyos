#include "io/nl/expressions.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/node.hpp"
#include "io/lines.hpp"
#include "io/nl/operators.hpp"
#include "io/nl/pieces.hpp"
#include "io/nl/tokens.hpp"
#include "io/records.hpp"
#include "io/report.hpp"
#include "model/model.hpp"

#include <array>
#include <cstddef>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace biyos::nl
{

namespace
{

constexpr std::array kPlus{1.0, 1.0};
constexpr std::array kMinus{1.0, -1.0};
constexpr std::array kUnit{1.0};
constexpr std::array kLog10{std::numbers::log10e};
constexpr std::string_view kPlterm = "a plterm";

std::string operator_text(const Opcode &opcode)
{
    return "operator " + std::string(opcode.name);
}

const Opcode &opcode_of(const Lines &lines, std::string_view field)
{
    const Opcode *opcode = find_opcode(integer_of(lines, field));
    if (opcode == nullptr)
    {
        bad_file(lines, "operator " + quote(field) + " is unknown");
    }
    return *opcode;
}

std::string_view leading(const Lines &lines) noexcept
{
    return fields_of(lines).front();
}

std::string_view single(const Lines &lines, const Entry &entry)
{
    check_size(lines, entry.size(), 1);
    return entry[0];
}

Index count_line(Lines &lines, std::string_view part)
{
    advance_in(lines, part);
    const Fields fields = fields_of(lines);
    check_size(lines, fields.size(), 1);
    return count_of(lines, fields[0]);
}

}

Expressions::Expressions(Lines &lines, Model &model, Index variables, Index defined)
    : lines_(lines), model_(model), variables_(variables), defined_(to_size(defined), kNoRoot)
{
}

Operand Expressions::read()
{
    do
    {
        advance_in(lines_, "an expression");
        take(Entry(lines_));
        close();
    } while (!frames_.empty());
    const Operand result = operands_.back();
    operands_.clear();
    return result;
}

Offset Expressions::node_of(const Operand &operand)
{
    if (!operand.value)
    {
        return operand.node;
    }
    return node(Operator::Constant, {}, std::span(&*operand.value, 1));
}

bool Expressions::has(Index variable) const noexcept
{
    return defined_[to_size(variable - variables_)] != kNoRoot;
}

void Expressions::define(Index variable, std::span<const LinearTerm> terms, const Operand &body)
{
    Offset &slot = defined_[to_size(variable - variables_)];
    if (terms.empty())
    {
        slot = node_of(body);
        return;
    }
    children_.clear();
    values_.assign(1, body.value.value_or(0.0));
    for (const LinearTerm &term : terms)
    {
        children_.push_back(
            checked(lines_, [&] { return model_.add_variable_node(term.variable); }));
        values_.push_back(term.coefficient);
    }
    if (!body.value)
    {
        children_.push_back(body.node);
        values_.push_back(1.0);
    }
    slot = node(Operator::Sum, children_, values_);
}

void Expressions::take(const Entry &entry)
{
    switch (entry.key())
    {
    case 'n':
        operands_.push_back({.node = kNoRoot, .value = value_of(lines_, single(lines_, entry))});
        return;
    case 'v':
        operands_.push_back({.node = variable_node(single(lines_, entry)), .value = std::nullopt});
        return;
    case 'o':
        open(opcode_of(lines_, single(lines_, entry)));
        return;
    case 'f':
        unsupported(lines_, "imported functions");
    case 'h':
        unsupported(lines_, "strings");
    default:
        break;
    }
    bad_file(lines_, "expected an expression, not " + quote(leading(lines_)));
}

void Expressions::open(const Opcode &opcode)
{
    if (opcode.rule == Rule::Refused)
    {
        unsupported(lines_, operator_text(opcode));
    }
    if (opcode.rule == Rule::Pieces)
    {
        operands_.push_back({.node = piecewise(), .value = std::nullopt});
        return;
    }
    const std::size_t needed = operand_count(opcode);
    frames_.push_back({.opcode = &opcode, .first = operands_.size(), .needed = needed});
}

void Expressions::close()
{
    while (!frames_.empty() && operands_.size() - frames_.back().first == frames_.back().needed)
    {
        const Frame frame = frames_.back();
        frames_.pop_back();
        const Offset top = apply(*frame.opcode, std::span(operands_).subspan(frame.first));
        operands_.resize(frame.first);
        operands_.push_back({.node = top, .value = std::nullopt});
    }
}

std::size_t Expressions::operand_count(const Opcode &opcode)
{
    switch (opcode.arity)
    {
    case Arity::One:
        return 1;
    case Arity::Two:
        return 2;
    case Arity::Many:
        break;
    }
    const Index count = count_line(lines_, operator_text(opcode));
    if (count == 0)
    {
        bad_file(lines_, operator_text(opcode) + " has no operands");
    }
    return to_size(count);
}

Offset Expressions::apply(const Opcode &opcode, std::span<const Operand> operands)
{
    switch (opcode.rule)
    {
    case Rule::Same:
        gather(operands);
        return node(opcode.op, children_, {});
    case Rule::Plus:
        return sum_of(operands, kPlus);
    case Rule::Minus:
        return sum_of(operands, kMinus);
    case Rule::Sum:
        return sum_of(operands, ones(operands.size()));
    case Rule::Times:
        gather(operands);
        return node(Operator::Product, children_, kUnit);
    case Rule::Raise:
        return power_of(operands[0], operands[1]);
    case Rule::Log10:
        return log10_of(operands[0]);
    case Rule::Pieces:
    case Rule::Refused:
        break;
    }
    throw Error(ErrorKind::Internal, "io", operator_text(opcode) + " takes no operands");
}

void Expressions::gather(std::span<const Operand> operands)
{
    children_.clear();
    for (const Operand &operand : operands)
    {
        children_.push_back(node_of(operand));
    }
}

std::span<const Real> Expressions::ones(std::size_t n)
{
    if (ones_.size() < n)
    {
        ones_.resize(n, 1.0);
    }
    return std::span(ones_).first(n);
}

Offset Expressions::sum_of(std::span<const Operand> operands, std::span<const Real> weights)
{
    gather(operands);
    values_.assign(1, 0.0);
    values_.insert(values_.end(), weights.begin(), weights.end());
    return node(Operator::Sum, children_, values_);
}

Offset Expressions::power_of(const Operand &base, const Operand &exponent)
{
    if (!exponent.value)
    {
        gather(std::array{base, exponent});
        return node(Operator::Power, children_, {});
    }
    const Offset child = node_of(base);
    return node(Operator::ConstantPower, std::span(&child, 1), std::span(&*exponent.value, 1));
}

Offset Expressions::log10_of(const Operand &operand)
{
    const Offset child = node_of(operand);
    const Offset log = node(Operator::Log, std::span(&child, 1), {});
    return node(Operator::Product, std::span(&log, 1), kLog10);
}

Offset Expressions::node(Operator op,
                         std::span<const Offset> children,
                         std::span<const Real> values)
{
    const int code = static_cast<int>(op);
    return checked(lines_, [&] { return model_.add_node(code, children, values); });
}

Offset Expressions::piecewise()
{
    const Index count = count_line(lines_, kPlterm);
    if (count == 0)
    {
        bad_file(lines_, "a plterm has no slopes");
    }
    slopes_.clear();
    breaks_.clear();
    for (Index k = 1; k < count; ++k)
    {
        slopes_.push_back(piece());
        breaks_.push_back(piece());
    }
    slopes_.push_back(piece());
    advance_in(lines_, kPlterm);
    const Entry entry(lines_);
    if (entry.key() != 'v')
    {
        bad_file(lines_, "a plterm ends with a variable, not " + quote(leading(lines_)));
    }
    const Offset child = variable_node(single(lines_, entry));
    const Curve curve = curve_of(lines_, slopes_, breaks_);
    const Offset pwl =
        checked(lines_, [&] { return model_.add_piecewise_linear(curve.x, curve.y); });
    return checked(lines_, [&] { return model_.add_piecewise(pwl, child); });
}

Real Expressions::piece()
{
    advance_in(lines_, kPlterm);
    const Entry entry(lines_);
    if (entry.key() != 'n')
    {
        bad_file(lines_, "a plterm holds numbers, not " + quote(leading(lines_)));
    }
    return value_of(lines_, single(lines_, entry));
}

Offset Expressions::variable_node(std::string_view field)
{
    const Index total = to_index(to_size(variables_) + defined_.size());
    const Index variable = index_in(lines_, field, total, "variable");
    if (variable < variables_)
    {
        return checked(lines_, [&] { return model_.add_variable_node(variable); });
    }
    const Offset defined = defined_[to_size(variable - variables_)];
    if (defined == kNoRoot)
    {
        bad_file(lines_,
                 "defined variable " + std::string(field) + " is used before its definition");
    }
    return defined;
}

}
