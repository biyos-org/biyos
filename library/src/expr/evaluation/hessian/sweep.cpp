#include "expr/evaluation/hessian/sweep.hpp"

#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "expr/evaluation/derivatives/partials.hpp"
#include "expr/evaluation/derivatives/reverse.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/evaluation/hessian/tangents.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::size_t kSingle = 1;

struct Lift
{
    Frame frame;
    Tangents tangents;
    std::span<const std::uint32_t> places;
    std::span<Real> partials;
    std::span<Real> curves;
    std::span<Real> result;
    std::span<const Real> seeds;
};

struct Trail
{
    std::span<const std::uint32_t> operands;
    std::span<const Real> weights;
    std::span<const std::uint32_t> places;
    std::span<const Real> partials;
    std::span<const Real> curves;
    std::span<Real> adjoints;
    std::span<Real> tangents;
    std::span<Real> columns;
    std::span<Real> taken;
    std::span<Real> held;
    std::size_t lanes;
};

template <typename T>
std::span<T> mine(std::span<T> region,
                  std::span<const std::uint32_t> places,
                  std::size_t k,
                  std::size_t lanes) noexcept
{
    return region.subspan(places[k] * lanes, (places[k + 1] - places[k]) * lanes);
}

Lift lift_of(const Block &block, std::span<const Real> point, Pass &pass) noexcept
{
    const std::size_t lanes = pass.lanes;
    Workspace &workspace = pass.workspace;
    return {
        .frame =
            {
                .point = point,
                .operands = block.operands,
                .weights = block.weights,
                .slots = workspace.slots,
                .result = workspace.result,
                .spare = workspace.spare,
                .lanes = kSingle,
            },
        .tangents =
            {
                .slots = pass.velocity.slots,
                .running = pass.velocity.running,
                .prefix = pass.velocity.prefix,
                .lanes = lanes,
            },
        .places = block.places,
        .partials = region(block, std::span(workspace.partials), kSingle),
        .curves = region(block, std::span(pass.velocity.partials), lanes),
        .result = pass.velocity.result,
        .seeds = pass.seeds,
    };
}

std::span<const Real> tangent(const Lift &lift, std::uint32_t slot) noexcept
{
    return row(std::span<const Real>(lift.tangents.slots), slot, lift.tangents.lanes);
}

void add(const Lift &lift, std::span<const Real> tangents, Real partial) noexcept
{
    for (std::size_t l = 0; l < lift.tangents.lanes; ++l)
    {
        lift.result[l] += scaled(tangents[l], partial);
    }
}

void add_sum(const Instruction &instruction,
             const Lift &lift,
             std::span<const Real> partials) noexcept
{
    const std::span<const std::uint32_t> terms = listed(instruction, lift.frame.operands);
    const std::span<const Real> weights =
        lift.frame.weights.subspan(place(instruction.payload) + 1, terms.size());
    for (std::size_t k = 0; k < terms.size(); ++k)
    {
        add(lift, tangent(lift, terms[k]), partials[0] * weights[k]);
    }
}

void add_product(const Instruction &instruction,
                 const Lift &lift,
                 std::span<const Real> partials) noexcept
{
    const std::span<const std::uint32_t> factors = listed(instruction, lift.frame.operands);
    for (std::size_t k = 0; k < factors.size(); ++k)
    {
        add(lift, tangent(lift, factors[k]), partials[k]);
    }
}

void pick(Real &into, std::size_t k, Real from, Real partial) noexcept
{
    if (is_zero(from))
    {
        return;
    }
    if (std::isnan(partial))
    {
        into += from * partial;
    }
    else if (chosen(partial) == k)
    {
        into += from;
    }
}

void pick(std::span<Real> into, std::size_t k, std::span<const Real> from, Real partial) noexcept
{
    for (std::size_t l = 0; l < into.size(); ++l)
    {
        pick(into[l], k, from[l], partial);
    }
}

void add_extreme(const Instruction &instruction,
                 const Lift &lift,
                 std::span<const Real> partials) noexcept
{
    const std::span<const std::uint32_t> operands = listed(instruction, lift.frame.operands);
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        pick(lift.result, k, tangent(lift, operands[k]), partials[0]);
    }
}

void motion(const Instruction &instruction,
            const Lift &lift,
            std::span<const Real> partials) noexcept
{
    const std::size_t lanes = lift.tangents.lanes;
    std::ranges::fill(lift.result, 0.0);
    switch (instruction.op)
    {
    case Operator::Constant:
        return;
    case Operator::Variable:
        std::ranges::copy(row(lift.seeds, place(instruction.payload), lanes), lift.result.begin());
        return;
    case Operator::Sum:
        add_sum(instruction, lift, partials);
        return;
    case Operator::Product:
        add_product(instruction, lift, partials);
        return;
    case Operator::Division:
    case Operator::Power:
    case Operator::Atan2:
        add(lift, tangent(lift, instruction.first), partials[0]);
        add(lift, tangent(lift, instruction.second), partials[1]);
        return;
    case Operator::Negation:
    case Operator::ConstantPower:
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
    case Operator::PiecewiseLinear:
        add(lift, tangent(lift, instruction.first), partials[0]);
        return;
    case Operator::Max:
    case Operator::Min:
        add_extreme(instruction, lift, partials);
        return;
    }
}

void advance(const Instruction &instruction, std::size_t k, const Lift &lift) noexcept
{
    const std::size_t lanes = lift.tangents.lanes;
    const std::span<Real> partials = mine(lift.partials, lift.places, k, kSingle);
    const std::span<Real> curves = mine(lift.curves, lift.places, k, lanes);
    evaluate(instruction, lift.frame);
    partials_of(instruction, lift.frame, partials);
    partial_tangents(instruction, lift.frame, lift.tangents, partials, curves);
    motion(instruction, lift, partials);
    keep(instruction, lift.frame);
    std::ranges::copy(lift.result, row(lift.tangents.slots, instruction.target, lanes).begin());
}

void copy_row(std::span<const Real> from,
              std::size_t source,
              std::span<Real> into,
              std::size_t target,
              std::size_t lanes) noexcept
{
    std::ranges::copy(row(from, source, lanes), row(into, target, lanes).begin());
}

void load(const Block &block, std::size_t output, Pass &pass) noexcept
{
    const std::size_t lanes = pass.lanes;
    for (const Run run : slice(std::span(block.reads), block.read_starts, output))
    {
        for (std::uint32_t k = run.begin; k < run.end; ++k)
        {
            const std::size_t from = to_size(block.loads[k]);
            pass.workspace.slots[k] = pass.workspace.shared[from];
            copy_row(pass.velocity.shared, from, pass.velocity.slots, k, lanes);
        }
    }
}

void lift(const Block &block, std::size_t output, std::span<const Real> point, Pass &pass) noexcept
{
    load(block, output, pass);
    const Lift lift = lift_of(block, point, pass);
    for (const Run run : slice(std::span(block.runs), block.starts, output))
    {
        for (std::uint32_t k = run.begin; k < run.end; ++k)
        {
            advance(block.instructions[k], k, lift);
        }
    }
}

void hold(const Block &block, std::size_t output, Pass &pass) noexcept
{
    const std::size_t lanes = pass.lanes;
    const std::size_t position = to_size(block.begin) + output;
    const std::uint32_t slot = block.outputs[output];
    pass.workspace.shared[position] = pass.workspace.slots[slot];
    copy_row(pass.velocity.slots, slot, pass.velocity.shared, position, lanes);
}

void rise(const Tape &tape, Offset element, std::span<const Real> point, Pass &pass) noexcept
{
    const std::span<const Block> prepass = tape.blocks().first(to_size(tape.prepass()));
    for (const Range range : tape.route(element))
    {
        for (Offset position = range.begin; position < range.end; ++position)
        {
            const Block &block = block_of(prepass, position);
            const std::size_t output = to_size(position - block.begin);
            lift(block, output, point, pass);
            hold(block, output, pass);
        }
    }
}

Trail trail_of(const Block &block, Pass &pass) noexcept
{
    const std::size_t lanes = pass.lanes;
    return {
        .operands = block.operands,
        .weights = block.weights,
        .places = block.places,
        .partials = region(block, std::span<const Real>(pass.workspace.partials), kSingle),
        .curves = region(block, std::span<const Real>(pass.velocity.partials), lanes),
        .adjoints = pass.first.slots,
        .tangents = pass.second.slots,
        .columns = pass.second.gradient,
        .taken = pass.first.taken,
        .held = pass.second.taken,
        .lanes = lanes,
    };
}

void take(const Instruction &instruction, const Trail &trail) noexcept
{
    const std::span<Real> tangent = row(trail.tangents, instruction.target, trail.lanes);
    trail.taken[0] = std::exchange(trail.adjoints[instruction.target], 0.0);
    std::ranges::copy(tangent, trail.held.begin());
    std::ranges::fill(tangent, 0.0);
}

void both(std::uint32_t slot,
          Real partial,
          std::span<const Real> curves,
          const Trail &trail) noexcept
{
    const Real taken = trail.taken[0];
    const std::span<Real> tangent = row(trail.tangents, slot, trail.lanes);
    trail.adjoints[slot] += scaled(taken, partial);
    for (std::size_t l = 0; l < trail.lanes; ++l)
    {
        tangent[l] += scaled(trail.held[l], partial) + scaled(taken, curves[l]);
    }
}

void spread_sum(const Instruction &instruction,
                std::span<const Real> partials,
                const Trail &trail) noexcept
{
    const std::span<const std::uint32_t> terms = listed(instruction, trail.operands);
    const std::span<const Real> weights =
        trail.weights.subspan(place(instruction.payload) + 1, terms.size());
    for (std::size_t k = 0; k < terms.size(); ++k)
    {
        const Real partial = partials[0] * weights[k];
        const std::span<Real> tangent = row(trail.tangents, terms[k], trail.lanes);
        trail.adjoints[terms[k]] += scaled(trail.taken[0], partial);
        for (std::size_t l = 0; l < trail.lanes; ++l)
        {
            tangent[l] += scaled(trail.held[l], partial);
        }
    }
}

void spread(const Instruction &instruction,
            std::span<const Real> partials,
            std::span<const Real> curves,
            const Trail &trail) noexcept
{
    const std::span<const std::uint32_t> operands = listed(instruction, trail.operands);
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        both(operands[k], partials[k], row(curves, k, trail.lanes), trail);
    }
}

void send(const Instruction &instruction,
          std::span<const Real> partials,
          const Trail &trail) noexcept
{
    const std::span<const std::uint32_t> operands = listed(instruction, trail.operands);
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        pick(trail.adjoints[operands[k]], k, trail.taken[0], partials[0]);
        pick(row(trail.tangents, operands[k], trail.lanes), k, trail.held, partials[0]);
    }
}

void column(const Instruction &instruction, const Trail &trail) noexcept
{
    const std::span<Real> into = row(trail.columns, place(instruction.payload), trail.lanes);
    for (std::size_t l = 0; l < trail.lanes; ++l)
    {
        into[l] += trail.held[l];
    }
}

void step(const Instruction &instruction,
          std::span<const Real> partials,
          std::span<const Real> curves,
          const Trail &trail) noexcept
{
    const std::size_t lanes = trail.lanes;
    take(instruction, trail);
    switch (instruction.op)
    {
    case Operator::Constant:
        return;
    case Operator::Variable:
        column(instruction, trail);
        return;
    case Operator::Sum:
        spread_sum(instruction, partials, trail);
        return;
    case Operator::Product:
        spread(instruction, partials, curves, trail);
        return;
    case Operator::Division:
    case Operator::Power:
    case Operator::Atan2:
        both(instruction.first, partials[0], row(curves, 0, lanes), trail);
        both(instruction.second, partials[1], row(curves, 1, lanes), trail);
        return;
    case Operator::Negation:
    case Operator::ConstantPower:
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
    case Operator::PiecewiseLinear:
        both(instruction.first, partials[0], curves, trail);
        return;
    case Operator::Max:
    case Operator::Min:
        send(instruction, partials, trail);
        return;
    }
}

void release(const Block &block, Run loads, Pass &pass) noexcept
{
    const std::size_t lanes = pass.lanes;
    for (std::uint32_t load = loads.begin; load < loads.end; ++load)
    {
        const std::size_t into = to_size(block.loads[load]);
        pass.first.shared[into] += std::exchange(pass.first.slots[load], 0.0);
        drain(row(std::span(pass.second.slots), load, lanes),
              row(std::span(pass.second.shared), into, lanes));
    }
}

void retrace(const Block &block, std::size_t output, Pass &pass) noexcept
{
    const Trail trail = trail_of(block, pass);
    const std::size_t lanes = pass.lanes;
    for (const Run run : std::views::reverse(slice(std::span(block.runs), block.starts, output)))
    {
        for (std::uint32_t k = run.end; k > run.begin; --k)
        {
            step(block.instructions[k - 1],
                 mine(trail.partials, trail.places, k - 1, kSingle),
                 mine(trail.curves, trail.places, k - 1, lanes),
                 trail);
        }
    }
    for (const Run run : slice(std::span(block.reads), block.read_starts, output))
    {
        release(block, run, pass);
    }
}

void fall(const Block &block, std::size_t output, Pass &pass) noexcept
{
    const std::size_t lanes = pass.lanes;
    const std::size_t position = to_size(block.begin) + output;
    const std::uint32_t slot = block.outputs[output];
    pass.first.slots[slot] += std::exchange(pass.first.shared[position], 0.0);
    drain(row(std::span(pass.second.shared), position, lanes),
          row(std::span(pass.second.slots), slot, lanes));
    retrace(block, output, pass);
}

void descend(const Tape &tape, Offset element, Pass &pass) noexcept
{
    const std::span<const Block> prepass = tape.blocks().first(to_size(tape.prepass()));
    for (const Range range : std::views::reverse(tape.route(element)))
    {
        for (Offset position = range.end; position > range.begin; --position)
        {
            const Block &block = block_of(prepass, position - 1);
            fall(block, to_size(position - 1 - block.begin), pass);
        }
    }
}

}

Velocity::Velocity(const Tape &tape, std::size_t lanes)
    : slots(sized(tape.width(), lanes), 0.0), shared(sized(to_size(tape.pinned()), lanes), 0.0),
      partials(sized(to_size(tape.partials()), lanes), 0.0), running(lanes, 0.0),
      prefix(widest_product(tape), 0.0), result(lanes, 0.0)
{
}

Pass::Pass(const Tape &tape, std::size_t count)
    : lanes(lanes_of(tape, count)), workspace(tape), velocity(tape, lanes), first(tape),
      second(tape, lanes), seeds(sized(to_size(tape.variables()), lanes), 0.0)
{
}

void sweep(const Tape &tape, Offset element, std::span<const Real> point, Pass &pass) noexcept
{
    const std::span<const Block> partition = tape.blocks().subspan(to_size(tape.prepass()));
    const Block &block = block_of(partition, element);
    const std::size_t output = to_size(element - block.begin);
    rise(tape, element, point, pass);
    lift(block, output, point, pass);
    pass.first.slots[block.outputs[output]] = 1.0;
    retrace(block, output, pass);
    descend(tape, element, pass);
}

}
