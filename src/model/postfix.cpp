#include "model/postfix.hpp"

#include "biyos/biyos.h"
#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "expr/builder.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"
#include "model/checks.hpp"
#include "model/nodes.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace biyos
{

namespace
{

[[noreturn]] void refuse_entry(const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, "expression", reason);
}

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

std::string operand_text(Offset n)
{
    return n_text(n, "operand", "operands");
}

Offset arity_of(Operator op, Offset argument) noexcept
{
    return op == Operator::Variable ? 0 : argument;
}

class Decoder
{
  public:
    Decoder(Graph &graph, Index variables, const Postfix &postfix);

    Offset expression(std::size_t k);

  private:
    void read(std::size_t k, std::size_t e);
    void step(int code, Offset argument);
    void take(Operator op, Offset arity);
    void build(Operator op, Offset argument);
    void node(Offset id);
    void variable(Offset variable);
    std::span<const double> values(Offset n);

    Builder builder_;
    Index n_variables_;
    Offset n_nodes_;
    const Postfix &postfix_;
    std::size_t next_ = 0;
    Offset depth_ = 0;
};

Decoder::Decoder(Graph &graph, Index variables, const Postfix &postfix)
    : builder_(graph), n_variables_(variables), n_nodes_(graph.size()), postfix_(postfix)
{
}

Offset Decoder::expression(std::size_t k)
{
    depth_ = 0;
    const auto end = at(postfix_.starts[k + 1]);
    for (auto e = at(postfix_.starts[k]); e < end; ++e)
    {
        read(k, e);
    }
    if (depth_ != 1)
    {
        const std::string reason = operand_text(depth_) + " left, not 1";
        throw Error(ErrorKind::InvalidArgument, item("expression", static_cast<Index>(k)), reason);
    }
    return builder_.finish();
}

void Decoder::read(std::size_t k, std::size_t e)
{
    try
    {
        step(postfix_.codes[e], postfix_.arguments[e]);
    }
    catch (const Error &error)
    {
        if (error.kind() != ErrorKind::InvalidArgument)
        {
            throw;
        }
        const std::string reason = "entry " + text_of(e) + ": " + error.message();
        throw Error(ErrorKind::InvalidArgument, item("expression", static_cast<Index>(k)), reason);
    }
}

void Decoder::step(int code, Offset argument)
{
    if (code == BIYOS_OP_NODE)
    {
        node(argument);
        return;
    }
    const Operator op = to_operator(code);
    take(op, arity_of(op, argument));
    build(op, argument);
}

void Decoder::take(Operator op, Offset arity)
{
    check_shape(op, payload_of(op), arity);
    if (arity > depth_)
    {
        refuse_entry(operand_text(arity) + " taken, " + text_of(depth_) + " given");
    }
    depth_ += 1 - arity;
}

void Decoder::build(Operator op, Offset argument)
{
    switch (op)
    {
    case Operator::Variable:
        variable(argument);
        return;
    case Operator::Constant:
        builder_.constant(values(1).front());
        return;
    case Operator::Sum:
    {
        const std::span<const double> read = values(argument + 1);
        builder_.sum(read.front(), read.subspan(1));
        return;
    }
    case Operator::Product:
        builder_.product(values(1).front(), argument);
        return;
    case Operator::ConstantPower:
        builder_.power(values(1).front());
        return;
    case Operator::SignPower:
        builder_.sign_power(values(1).front());
        return;
    case Operator::Negation:
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
    case Operator::Erf:
        builder_.apply(op, argument);
        return;
    }
}

void Decoder::node(Offset id)
{
    check_child(id, n_nodes_);
    builder_.node(id);
    ++depth_;
}

void Decoder::variable(Offset variable)
{
    if (variable < 0 || variable >= n_variables_)
    {
        refuse_entry("variable " + text_of(variable) + " is unknown");
    }
    builder_.variable(static_cast<Index>(variable));
}

std::span<const double> Decoder::values(Offset n)
{
    const std::size_t left = postfix_.values.size() - next_;
    if (at(n) > left)
    {
        throw Error(ErrorKind::Internal, "postfix", "values run out");
    }
    const std::span<const double> read = postfix_.values.subspan(next_, at(n));
    next_ += at(n);
    return read;
}

Offset clamped(Offset argument, Offset most) noexcept
{
    return (std::clamp)(argument, Offset{0}, most);
}

}

Offset n_values(std::span<const int> codes, std::span<const Offset> arguments)
{
    const auto entries = static_cast<Offset>(codes.size());
    const Offset most = 2 * entries;
    Offset total = 0;
    for (std::size_t e = 0; e < codes.size() && total < most; ++e)
    {
        total += n_values(codes[e], clamped(arguments[e], entries));
    }
    return (std::min)(total, most);
}

std::vector<Offset> decode(Graph &graph, Index variables, const Postfix &postfix)
{
    Decoder decoder(graph, variables, postfix);
    std::vector<Offset> tops;
    tops.reserve(postfix.starts.size() - 1);
    for (std::size_t k = 0; k + 1 < postfix.starts.size(); ++k)
    {
        tops.push_back(decoder.expression(k));
    }
    return tops;
}

}
