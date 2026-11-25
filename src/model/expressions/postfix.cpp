#include "model/expressions/postfix.hpp"

#include "biyos/biyos.h"
#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/graph/builder.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "model/checks.hpp"
#include "model/expressions/nodes.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

[[noreturn]] void refuse_entry(const std::string &reason)
{
    throw Error(ErrorKind::InvalidArgument, "expression", reason);
}

std::string operand_text(Offset n)
{
    return n_text(n, "operand", "operands");
}

Offset arity_of(Operator op, Offset argument) noexcept
{
    if (op == Operator::Variable)
    {
        return 0;
    }
    return op == Operator::PiecewiseLinear ? 1 : argument;
}

Offset argument_of(const Graph &graph, Offset node) noexcept
{
    if (graph.op(node) == Operator::PiecewiseLinear)
    {
        return graph.piecewise_linear(node);
    }
    return to_offset(graph.children(node).size());
}

struct Kept
{
    Offset entry;
    Offset node;
};

class Decoder
{
  public:
    Decoder(Graph &graph, Index variables, const Postfix &postfix);

    Offset expression(std::size_t k);

  private:
    void read(std::size_t k, std::size_t e);
    void step(std::size_t e, int code, Offset argument);
    void take(Operator op, Offset arity);
    void pop(Offset arity);
    void build(Operator op, Offset argument);
    void keep(std::size_t e, Offset arity);
    void reference(Offset entry);
    void node(Offset id);
    void variable(Offset variable);
    std::span<const double> values(Offset n);

    Builder builder_;
    Index n_variables_;
    Offset n_nodes_;
    const Postfix &postfix_;
    std::vector<Kept> kept_;
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
    const auto end = to_size(postfix_.starts[k + 1]);
    for (auto e = to_size(postfix_.starts[k]); e < end; ++e)
    {
        read(k, e);
    }
    if (depth_ != 1)
    {
        const std::string reason = operand_text(depth_) + " left, not 1";
        throw Error(ErrorKind::InvalidArgument, item("expression", to_index(k)), reason);
    }
    return builder_.finish();
}

void Decoder::read(std::size_t k, std::size_t e)
{
    try
    {
        step(e, postfix_.codes[e], postfix_.arguments[e]);
    }
    catch (const Error &error)
    {
        if (error.kind() != ErrorKind::InvalidArgument)
        {
            throw;
        }
        const std::string reason = "entry " + text_of(e) + ": " + error.message();
        throw Error(ErrorKind::InvalidArgument, item("expression", to_index(k)), reason);
    }
}

void Decoder::step(std::size_t e, int code, Offset argument)
{
    if (code == BIYOS_OP_NODE)
    {
        node(argument);
        return;
    }
    if (code == BIYOS_OP_KEEP)
    {
        keep(e, argument);
        return;
    }
    if (code == BIYOS_OP_REF)
    {
        reference(argument);
        return;
    }
    const Operator op = to_operator(code);
    take(op, arity_of(op, argument));
    build(op, argument);
}

void Decoder::take(Operator op, Offset arity)
{
    check_shape(op, payload_of(op), arity);
    pop(arity);
}

void Decoder::pop(Offset arity)
{
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
    case Operator::PiecewiseLinear:
        builder_.piecewise(argument);
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

void Decoder::keep(std::size_t e, Offset arity)
{
    if (arity != 1)
    {
        const std::string wanted = "operator " + text_of(BIYOS_OP_KEEP) + " takes 1 child";
        refuse_entry(wanted + ", not " + text_of(arity));
    }
    pop(arity);
    kept_.push_back(Kept{
        .entry = to_offset(e),
        .node = builder_.keep(),
    });
}

void Decoder::reference(Offset entry)
{
    const auto found = std::ranges::lower_bound(kept_, entry, {}, &Kept::entry);
    if (found == kept_.end() || found->entry != entry)
    {
        refuse_entry("entry " + text_of(entry) + " is no earlier keep");
    }
    builder_.node(found->node);
    ++depth_;
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
    if (to_size(n) > left)
    {
        throw Error(ErrorKind::Internal, "postfix", "values run out");
    }
    const std::span<const double> read = postfix_.values.subspan(next_, to_size(n));
    next_ += to_size(n);
    return read;
}

Offset clamped(Offset argument, Offset most) noexcept
{
    return (std::clamp)(argument, Offset{0}, most);
}

constexpr Offset kUnwritten = -1;

struct Frame
{
    Offset node;
    bool kept;
    std::size_t next;
};

bool merges(Operator parent, Operator child) noexcept
{
    return parent == child && (child == Operator::Sum || child == Operator::Product);
}

class Encoder
{
  public:
    explicit Encoder(const Graph &graph);

    Encoding encode(std::span<const Offset> roots);

  private:
    void count(Offset root);
    void reach(Offset node);
    void expression(Offset root);
    void step();
    void meet(Offset node, bool nested);
    void finish(const Frame &frame);
    void put(int code, Offset argument);
    void put_values(Offset node);

    const Graph &graph_;
    std::vector<Offset> meetings_;
    std::vector<Offset> positions_;
    std::vector<Offset> pending_;
    std::vector<Frame> frames_;
    Encoding encoding_;
};

Encoder::Encoder(const Graph &graph)
    : graph_(graph), meetings_(to_size(graph.size()), 0),
      positions_(to_size(graph.size()), kUnwritten)
{
}

Encoding Encoder::encode(std::span<const Offset> roots)
{
    for (const Offset root : roots)
    {
        count(root);
    }
    encoding_.starts.push_back(0);
    for (const Offset root : roots)
    {
        expression(root);
        encoding_.starts.push_back(to_offset(encoding_.codes.size()));
    }
    return std::move(encoding_);
}

void Encoder::count(Offset root)
{
    if (root == kNoRoot)
    {
        return;
    }
    reach(root);
    while (!pending_.empty())
    {
        const Offset node = pending_.back();
        pending_.pop_back();
        for (const Offset child : graph_.children(node))
        {
            reach(child);
        }
    }
}

void Encoder::reach(Offset node)
{
    ++meetings_[to_size(node)];
    if (meetings_[to_size(node)] == 1)
    {
        pending_.push_back(node);
    }
}

void Encoder::expression(Offset root)
{
    if (root == kNoRoot)
    {
        put(BIYOS_OP_CONST, 0);
        encoding_.values.push_back(0.0);
        return;
    }
    meet(root, false);
    while (!frames_.empty())
    {
        step();
    }
}

void Encoder::step()
{
    Frame &top = frames_.back();
    const std::span<const Offset> children = graph_.children(top.node);
    if (top.next == children.size())
    {
        const Frame done = top;
        frames_.pop_back();
        finish(done);
        return;
    }
    const Offset child = children[top.next];
    ++top.next;
    meet(child, merges(graph_.op(top.node), graph_.op(child)));
}

void Encoder::meet(Offset node, bool nested)
{
    if (graph_.op(node) == Operator::Variable)
    {
        put(BIYOS_OP_VAR, graph_.variable(node));
        return;
    }
    const Offset position = positions_[to_size(node)];
    if (position != kUnwritten)
    {
        put(BIYOS_OP_REF, position);
        return;
    }
    frames_.push_back(Frame{
        .node = node,
        .kept = nested || meetings_[to_size(node)] > 1,
        .next = 0,
    });
}

void Encoder::finish(const Frame &frame)
{
    const auto op = static_cast<int>(graph_.op(frame.node));
    put(op, argument_of(graph_, frame.node));
    put_values(frame.node);
    if (frame.kept)
    {
        positions_[to_size(frame.node)] = to_offset(encoding_.codes.size());
        put(BIYOS_OP_KEEP, 1);
    }
}

void Encoder::put(int code, Offset argument)
{
    encoding_.codes.push_back(code);
    encoding_.arguments.push_back(argument);
}

void Encoder::put_values(Offset node)
{
    switch (payload_of(graph_.op(node)))
    {
    case Payload::None:
    case Payload::Variable:
    case Payload::Points:
        return;
    case Payload::Constant:
        encoding_.values.push_back(graph_.constant(node));
        return;
    case Payload::Coefficients:
    {
        encoding_.values.push_back(graph_.constant(node));
        const std::span<const double> weights = graph_.coefficients(node);
        encoding_.values.insert(encoding_.values.end(), weights.begin(), weights.end());
        return;
    }
    }
}

}

Offset n_values(std::span<const int> codes, std::span<const Offset> arguments)
{
    const auto entries = to_offset(codes.size());
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

Encoding encode(const Graph &graph, std::span<const Offset> roots)
{
    Encoder encoder(graph);
    return encoder.encode(roots);
}

}
