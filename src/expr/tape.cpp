#include "expr/tape.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "expr/graph.hpp"
#include "expr/layout.hpp"
#include "expr/locator.hpp"
#include "expr/node.hpp"
#include "expr/plan.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

enum class Form
{
    Leaf,
    Direct,
    Listed,
};

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

Offset count_of(std::size_t value) noexcept
{
    return static_cast<Offset>(value);
}

Form form_of(Operator op) noexcept
{
    switch (op)
    {
    case Operator::Constant:
    case Operator::Variable:
        return Form::Leaf;
    case Operator::Sum:
    case Operator::Product:
    case Operator::Max:
    case Operator::Min:
        return Form::Listed;
    case Operator::Negation:
    case Operator::Division:
    case Operator::ConstantPower:
    case Operator::Power:
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
    case Operator::Entropy:
    case Operator::SignPower:
    case Operator::Erf:
        return Form::Direct;
    }
    return Form::Direct;
}

std::span<const Offset> part(std::span<const Offset> items,
                             std::span<const Offset> firsts,
                             std::size_t k) noexcept
{
    return items.subspan(at(firsts[k]), at(firsts[k + 1] - firsts[k]));
}

Offset variables_of(const Block &block) noexcept
{
    Offset count = 0;
    for (const Instruction &instruction : block.instructions)
    {
        if (instruction.op == Operator::Variable)
        {
            count = (std::max)(count, static_cast<Offset>(instruction.payload) + 1);
        }
    }
    return count;
}

class Compiler
{
  public:
    Compiler(const Graph &graph, const Plan &plan);

    std::vector<Block> blocks(std::span<const Offset> roots);

  private:
    Block compile(Offset index, std::span<const Offset> outputs, Offset first);
    Instruction instruction(Offset node, Block &block) const;
    std::uint64_t payload(Offset node, Block &block) const;
    std::uint64_t weights(Offset node, Block &block) const;

    const Graph &graph_;
    const Plan &plan_;
    Layout layout_;
    Locator locator_;
};

Compiler::Compiler(const Graph &graph, const Plan &plan)
    : graph_(graph), plan_(plan), layout_(graph, plan), locator_(graph, layout_)
{
}

std::vector<Block> Compiler::blocks(std::span<const Offset> roots)
{
    std::vector<Block> blocks;
    const Offset prepass = count_of(plan_.prepass.size()) - 1;
    for (std::size_t k = 0; k + 1 < plan_.prepass.size(); ++k)
    {
        const std::span<const Offset> outputs = part(plan_.pinned, plan_.prepass, k);
        blocks.push_back(compile(count_of(k), outputs, plan_.prepass[k]));
    }
    for (std::size_t k = 0; k + 1 < plan_.partitions.size(); ++k)
    {
        const std::span<const Offset> outputs = part(roots, plan_.partitions, k);
        Block partition = compile(prepass + count_of(k), outputs, plan_.partitions[k]);
        locator_.locate(outputs, partition);
        blocks.push_back(std::move(partition));
    }
    return blocks;
}

Block Compiler::compile(Offset index, std::span<const Offset> outputs, Offset first)
{
    layout_.build(index, outputs);
    Block block{
        .first = first,
        .width = layout_.width(),
        .instructions = {},
        .operands = {},
        .weights = {},
        .loads = {},
        .outputs = {},
        .intervals = {},
        .starts = {},
    };
    block.instructions.reserve(layout_.nodes().size());
    for (const Offset node : layout_.nodes())
    {
        block.instructions.push_back(instruction(node, block));
    }
    for (const Offset load : layout_.loads())
    {
        block.loads.push_back(plan_.position[at(load)]);
    }
    for (const Offset output : outputs)
    {
        block.outputs.push_back(layout_.slot(output));
    }
    return block;
}

Instruction Compiler::instruction(Offset node, Block &block) const
{
    const Operator op = graph_.op(node);
    const std::span<const Offset> children = graph_.children(node);
    Instruction result{
        .op = op,
        .target = layout_.slot(node),
        .first = 0,
        .second = 0,
        .payload = payload(node, block),
    };
    switch (form_of(op))
    {
    case Form::Leaf:
        break;
    case Form::Direct:
        result.first = layout_.slot(children.front());
        result.second = layout_.slot(children.back());
        break;
    case Form::Listed:
        result.first = narrow(block.operands.size());
        for (const Offset child : children)
        {
            block.operands.push_back(layout_.slot(child));
        }
        result.second = narrow(block.operands.size());
        break;
    }
    return result;
}

std::uint64_t Compiler::payload(Offset node, Block &block) const
{
    switch (payload_of(graph_.op(node)))
    {
    case Payload::None:
        return 0;
    case Payload::Constant:
        return std::bit_cast<std::uint64_t>(graph_.constant(node));
    case Payload::Variable:
        return static_cast<std::uint64_t>(graph_.variable(node));
    case Payload::Coefficients:
        return weights(node, block);
    }
    return 0;
}

std::uint64_t Compiler::weights(Offset node, Block &block) const
{
    const std::size_t position = block.weights.size();
    const std::span<const double> coefficients = graph_.coefficients(node);
    block.weights.push_back(graph_.constant(node));
    block.weights.insert(block.weights.end(), coefficients.begin(), coefficients.end());
    return position;
}

}

Tape::Tape(const Graph &graph, std::span<const Offset> roots, Offset budget)
    : roots_(count_of(roots.size()))
{
    for (const Offset root : roots)
    {
        check_child(root, graph.size());
    }
    const Plan plan = plan_of(graph, roots, budget);
    blocks_ = Compiler(graph, plan).blocks(roots);
    pinned_ = count_of(plan.pinned.size());
    prepass_ = count_of(plan.prepass.size()) - 1;
    for (const Block &block : blocks_)
    {
        variables_ = (std::max)(variables_, variables_of(block));
        width_ = (std::max)(width_, block.width);
    }
}

Offset Tape::roots() const noexcept
{
    return roots_;
}

Offset Tape::variables() const noexcept
{
    return variables_;
}

Offset Tape::pinned() const noexcept
{
    return pinned_;
}

std::uint32_t Tape::width() const noexcept
{
    return width_;
}

Offset Tape::prepass() const noexcept
{
    return prepass_;
}

std::span<const Block> Tape::blocks() const noexcept
{
    return blocks_;
}

std::span<const Interval> Tape::intervals(Offset root) const
{
    if (root < 0 || root >= roots_)
    {
        throw Error(ErrorKind::InvalidArgument, "tape", "root " + text_of(root) + " is unknown");
    }
    const std::span<const Block> partitions = std::span(blocks_).subspan(at(prepass_));
    const auto after = std::ranges::upper_bound(partitions, root, {}, &Block::first);
    const Block &block = *(after - 1);
    const std::size_t output = at(root - block.first);
    const Offset begin = block.starts[output];
    const Offset end = block.starts[output + 1];
    return std::span(block.intervals).subspan(at(begin), at(end - begin));
}

}
