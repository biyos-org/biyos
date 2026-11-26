#include "expr/tape/tape.hpp"

#include "core/error.hpp"
#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/derivatives/partials.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/layout.hpp"
#include "expr/tape/locator.hpp"
#include "expr/tape/plan.hpp"
#include "expr/tape/reads.hpp"

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

constexpr Offset kNone = -1;

enum class Form
{
    Leaf,
    Direct,
    Listed,
    Table,
};

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
    case Operator::Atan2:
        return Form::Direct;
    case Operator::PiecewiseLinear:
        return Form::Table;
    }
    return Form::Direct;
}

Offset variables_of(const Block &block) noexcept
{
    Offset n = 0;
    for (const Instruction &instruction : block.instructions)
    {
        if (instruction.op == Operator::Variable)
        {
            n = (std::max)(n, static_cast<Offset>(instruction.payload) + 1);
        }
    }
    return n;
}

std::vector<std::uint32_t> places_of(std::span<const Instruction> instructions)
{
    std::vector<std::uint32_t> places;
    places.reserve(instructions.size() + 1);
    std::size_t n = 0;
    for (const Instruction &instruction : instructions)
    {
        places.push_back(narrow(n));
        n += n_partials(instruction);
    }
    places.push_back(narrow(n));
    return places;
}

Offset set_origins(std::span<Block> blocks, Offset prepass) noexcept
{
    Offset total = 0;
    for (Block &block : blocks.first(to_size(prepass)))
    {
        block.origin = total;
        total += block.places.back();
    }
    Offset widest = 0;
    for (Block &block : blocks.subspan(to_size(prepass)))
    {
        block.origin = total;
        widest = (std::max)(widest, Offset{block.places.back()});
    }
    return total + widest;
}

class Compiler
{
  public:
    Compiler(const Graph &graph, const Plan &plan);

    std::vector<Block> blocks(std::span<const Offset> roots, const Reach &reach);

  private:
    Block compile(Offset index, std::span<const Offset> outputs, Offset start);
    Instruction instruction(Offset node, Block &block);
    std::uint64_t payload(Offset node, Block &block);
    std::uint64_t weights(Offset node, Block &block) const;
    std::uint64_t points(Offset node, Block &block);

    const Graph &graph_;
    const Plan &plan_;
    Layout layout_;
    Locator locator_;
    Offset index_ = kNone;
    std::vector<Offset> owners_;
    std::vector<std::uint64_t> places_;
};

Compiler::Compiler(const Graph &graph, const Plan &plan)
    : graph_(graph), plan_(plan), layout_(graph, plan), locator_(graph, plan.position, layout_),
      owners_(to_size(graph.points().size()), kNone), places_(to_size(graph.points().size()), 0)
{
}

std::vector<Block> Compiler::blocks(std::span<const Offset> roots, const Reach &reach)
{
    std::vector<Block> blocks;
    const Offset prepass = to_offset(plan_.prepass.size()) - 1;
    for (std::size_t k = 0; k + 1 < plan_.prepass.size(); ++k)
    {
        const std::span<const Offset> outputs = slice(std::span(plan_.pinned), plan_.prepass, k);
        Block pinned = compile(to_offset(k), outputs, plan_.prepass[k]);
        locator_.locate(outputs, pinned);
        read_children(graph_, layout_, outputs, pinned);
        blocks.push_back(std::move(pinned));
    }
    for (std::size_t k = 0; k + 1 < plan_.partitions.size(); ++k)
    {
        const std::span<const Offset> outputs = slice(roots, plan_.partitions, k);
        Block partition = compile(prepass + to_offset(k), outputs, plan_.partitions[k]);
        locator_.locate(outputs, partition);
        read_routes(reach, partition);
        blocks.push_back(std::move(partition));
    }
    return blocks;
}

Block Compiler::compile(Offset index, std::span<const Offset> outputs, Offset start)
{
    layout_.build(index, outputs);
    index_ = index;
    Block block{
        .begin = start,
        .width = layout_.width(),
        .instructions = {},
        .operands = {},
        .weights = {},
        .loads = {},
        .outputs = {},
        .runs = {},
        .starts = {},
        .reads = {},
        .read_starts = {},
        .places = {},
        .origin = 0,
    };
    block.instructions.reserve(layout_.nodes().size());
    for (const Offset node : layout_.nodes())
    {
        block.instructions.push_back(instruction(node, block));
    }
    for (const Offset load : layout_.loads())
    {
        block.loads.push_back(plan_.position[to_size(load)]);
    }
    for (const Offset output : outputs)
    {
        block.outputs.push_back(layout_.slot(output));
    }
    block.places = places_of(block.instructions);
    return block;
}

Instruction Compiler::instruction(Offset node, Block &block)
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
    case Form::Table:
        result.first = layout_.slot(children.front());
        result.second =
            narrow(graph_.points().piecewise_linear(graph_.piecewise_linear(node)).x.size());
        break;
    }
    return result;
}

std::uint64_t Compiler::payload(Offset node, Block &block)
{
    switch (payload_of(graph_.op(node)))
    {
    case Payload::None:
        return 0;
    case Payload::Constant:
        return std::bit_cast<Bits>(graph_.constant(node));
    case Payload::Variable:
        return static_cast<std::uint64_t>(graph_.variable(node));
    case Payload::Coefficients:
        return weights(node, block);
    case Payload::Points:
        return points(node, block);
    }
    return 0;
}

std::uint64_t Compiler::weights(Offset node, Block &block) const
{
    const std::size_t position = block.weights.size();
    const std::span<const Real> coefficients = graph_.coefficients(node);
    block.weights.push_back(graph_.constant(node));
    block.weights.insert(block.weights.end(), coefficients.begin(), coefficients.end());
    return position;
}

std::uint64_t Compiler::points(Offset node, Block &block)
{
    const std::size_t pwl = to_size(graph_.piecewise_linear(node));
    if (owners_[pwl] != index_)
    {
        const PiecewiseLinear table =
            graph_.points().piecewise_linear(graph_.piecewise_linear(node));
        owners_[pwl] = index_;
        places_[pwl] = block.weights.size();
        block.weights.insert(block.weights.end(), table.x.begin(), table.x.end());
        block.weights.insert(block.weights.end(), table.y.begin(), table.y.end());
    }
    return places_[pwl];
}

}

Tape::Tape(const Graph &graph, std::span<const Offset> roots, Offset budget)
    : roots_(to_offset(roots.size()))
{
    for (const Offset root : roots)
    {
        check_child(root, graph.size());
    }
    const Plan plan = plan_of(graph, roots, budget);
    reach_ = reach_of(graph, plan.position, roots);
    blocks_ = Compiler(graph, plan).blocks(roots, reach_);
    pinned_ = to_offset(plan.pinned.size());
    prepass_ = to_offset(plan.prepass.size()) - 1;
    partials_ = set_origins(blocks_, prepass_);
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

std::span<const Run> Tape::runs(Offset root) const
{
    if (root < 0 || root >= roots_)
    {
        throw Error(ErrorKind::InvalidArgument, "tape", "root " + text_of(root) + " is unknown");
    }
    const std::span<const Block> partitions = std::span(blocks_).subspan(to_size(prepass_));
    const auto after = std::ranges::upper_bound(partitions, root, {}, &Block::begin);
    const Block &block = *(after - 1);
    const std::size_t output = to_size(root - block.begin);
    return slice(std::span(block.runs), block.starts, output);
}

const Pattern &Tape::pattern() const noexcept
{
    return reach_.pattern;
}

std::span<const Range> Tape::route(Offset root) const noexcept
{
    const Offset begin = reach_.route.starts[to_size(root)];
    const Offset end = reach_.route.starts[to_size(root) + 1];
    return std::span(reach_.route.runs).subspan(to_size(begin), to_size(end - begin));
}

Offset Tape::partials() const noexcept
{
    return partials_;
}

}
