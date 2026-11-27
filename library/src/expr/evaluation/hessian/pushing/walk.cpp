#include "expr/evaluation/hessian/pushing/walk.hpp"

#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "expr/evaluation/derivatives/partials.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/evaluation/hessian/pattern.hpp"
#include "expr/evaluation/hessian/pushing/lists.hpp"
#include "expr/evaluation/hessian/tangents.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::size_t kSingle = 1;

struct Walk
{
    std::span<const Real> point;
    const Tolerances &tolerances;
    Workspace &workspace;
    const Elimination &elimination;
    PushSpace &space;
    std::size_t node;
};

void weigh(const Instruction &instruction,
           const Frame &frame,
           Real partial,
           std::span<Real> slopes) noexcept
{
    const std::span<const Real> weights =
        frame.weights.subspan(place(instruction.payload) + 1, slopes.size());
    for (std::size_t k = 0; k < slopes.size(); ++k)
    {
        slopes[k] = partial * weights[k];
    }
}

void pick(Real partial, std::span<Real> slopes) noexcept
{
    for (std::size_t k = 0; k < slopes.size(); ++k)
    {
        slopes[k] = std::isnan(partial) ? partial : (chosen(partial) == k ? 1.0 : 0.0);
    }
}

void slopes_of(const Instruction &instruction,
               const Frame &frame,
               std::span<const Real> partials,
               std::span<Real> slopes) noexcept
{
    switch (instruction.op)
    {
    case Operator::Sum:
        weigh(instruction, frame, partials[0], slopes);
        return;
    case Operator::Max:
    case Operator::Min:
        pick(partials[0], slopes);
        return;
    case Operator::Constant:
    case Operator::Variable:
    case Operator::Product:
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
    case Operator::Atan2:
    case Operator::Sinh:
    case Operator::Cosh:
    case Operator::Tanh:
    case Operator::Asinh:
    case Operator::Acosh:
    case Operator::Atanh:
    case Operator::Entropy:
    case Operator::SignPower:
    case Operator::Erf:
    case Operator::PiecewiseLinear:
        std::ranges::copy(partials.first(slopes.size()), slopes.begin());
        return;
    }
}

void full(Bends bends, std::span<Real> out) noexcept
{
    out[0] = bends.xx;
    if (out.size() > 1)
    {
        out[1] = bends.xy;
        out[2] = bends.yy;
    }
}

void quotient(Bends bends, std::span<Real> out) noexcept
{
    out[0] = bends.xy;
    out[1] = bends.yy;
}

void cross(const Instruction &instruction,
           const Frame &frame,
           std::span<Real> suffix,
           std::span<Real> out) noexcept
{
    const std::span<const std::uint32_t> factors = listed(instruction, frame.operands);
    const std::size_t n = factors.size();
    suffix[n] = 1.0;
    for (std::size_t k = n; k > 0; --k)
    {
        suffix[k - 1] = suffix[k] * frame.slots[factors[k - 1]];
    }
    std::size_t at = 0;
    Real prefix = constant_of(instruction);
    for (std::size_t k = 0; k < n; ++k)
    {
        Real middle = prefix;
        for (std::size_t m = k + 1; m < n; ++m)
        {
            out[at++] = middle * suffix[m + 1];
            middle *= frame.slots[factors[m]];
        }
        prefix *= frame.slots[factors[k]];
    }
}

void bend(const Instruction &instruction,
          const Frame &frame,
          std::span<const Real> partials,
          Bending bending,
          std::span<Real> suffix,
          std::span<Real> out) noexcept
{
    switch (bending)
    {
    case Bending::None:
        return;
    case Bending::Full:
        full(bends_of(instruction, frame, partials), out);
        return;
    case Bending::Quotient:
        quotient(bends_of(instruction, frame, partials), out);
        return;
    case Bending::Cross:
        cross(instruction, frame, suffix, out);
        return;
    }
}

void describe(const Instruction &instruction,
              const Frame &frame,
              std::span<const Real> partials,
              Walk &walk) noexcept
{
    const Elimination &elimination = walk.elimination;
    const Node &node = elimination.nodes[walk.node];
    const std::uint32_t first = elimination.bends[walk.node];
    const std::span<Real> slopes =
        std::span(walk.space.slopes).subspan(node.begin, node.end - node.begin);
    const std::span<Real> bends =
        std::span(walk.space.bends).subspan(first, elimination.bends[walk.node + 1] - first);
    slopes_of(instruction, frame, partials, slopes);
    bend(instruction, frame, partials, node.bending, walk.space.suffix, bends);
    mark_undefined(frame, bends);
    ++walk.node;
}

Frame frame_of(const Block &block, const Walk &walk) noexcept
{
    return {
        .point = walk.point,
        .operands = block.operands,
        .weights = block.weights,
        .slots = walk.workspace.slots,
        .result = walk.workspace.result,
        .spare = walk.workspace.spare,
        .lanes = kSingle,
        .tolerances = walk.tolerances,
    };
}

void enter(const Instruction &instruction,
           const Frame &frame,
           std::span<Real> partials,
           Walk &walk) noexcept
{
    evaluate(instruction, frame);
    partials_of(instruction, frame, partials);
    if (instruction.op != Operator::Variable)
    {
        describe(instruction, frame, partials, walk);
    }
    keep(instruction, frame);
}

void walk_block(const Block &block, std::size_t output, Walk &walk) noexcept
{
    Workspace &workspace = walk.workspace;
    for (const Run run : slice(std::span(block.reads), block.read_starts, output))
    {
        for (std::uint32_t k = run.begin; k < run.end; ++k)
        {
            workspace.slots[k] = workspace.shared[to_size(block.loads[k])];
        }
    }
    const Frame frame = frame_of(block, walk);
    const std::span<Real> partials = region(block, std::span(workspace.partials), kSingle);
    for (const Run run : slice(std::span(block.runs), block.starts, output))
    {
        for (std::uint32_t k = run.begin; k < run.end; ++k)
        {
            const std::span<Real> mine =
                partials.subspan(block.places[k], block.places[k + 1] - block.places[k]);
            enter(block.instructions[k], frame, mine, walk);
        }
    }
}

void rise(const Tape &tape, Walk &walk) noexcept
{
    const std::span<const Block> prepass = tape.blocks().first(to_size(tape.prepass()));
    for (const Range range : tape.route(walk.elimination.element))
    {
        for (Offset position = range.begin; position < range.end; ++position)
        {
            const Block &block = block_of(prepass, position);
            const std::size_t output = to_size(position - block.begin);
            walk_block(block, output, walk);
            walk.workspace.shared[to_size(position)] = walk.workspace.slots[block.outputs[output]];
        }
    }
}

}

void walk_element(const Tape &tape,
                  const Elimination &elimination,
                  std::span<const Real> point,
                  const Tolerances &tolerances,
                  Workspace &workspace,
                  PushSpace &space) noexcept
{
    Walk walk{
        .point = point,
        .tolerances = tolerances,
        .workspace = workspace,
        .elimination = elimination,
        .space = space,
        .node = 0,
    };
    rise(tape, walk);
    const std::span<const Block> partition = tape.blocks().subspan(to_size(tape.prepass()));
    const Block &block = block_of(partition, elimination.element);
    walk_block(block, to_size(elimination.element - block.begin), walk);
}

}
