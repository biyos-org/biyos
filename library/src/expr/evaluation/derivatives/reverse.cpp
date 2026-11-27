#include "expr/evaluation/derivatives/reverse.hpp"

#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "expr/evaluation/derivatives/partials.hpp"
#include "expr/evaluation/derivatives/value.hpp"
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

struct Trail
{
    std::span<const std::uint32_t> operands;
    std::span<const Real> weights;
    std::span<const std::uint32_t> places;
    std::span<const Real> partials;
    std::span<Real> slots;
    std::span<Real> gradient;
    std::span<Real> taken;
    std::size_t lanes;
};

Trail trail_of(const Block &block,
               const Workspace &workspace,
               Adjoints &adjoints,
               std::span<Real> gradient) noexcept
{
    return {
        .operands = block.operands,
        .weights = block.weights,
        .places = block.places,
        .partials = region(block, std::span(workspace.partials), workspace.lanes),
        .slots = adjoints.slots,
        .gradient = gradient,
        .taken = adjoints.taken,
        .lanes = workspace.lanes,
    };
}

std::span<Real> slot(const Trail &trail, std::uint32_t index) noexcept
{
    return row(trail.slots, index, trail.lanes);
}

std::span<const Real> partial(std::span<const Real> partials,
                              std::size_t k,
                              const Trail &trail) noexcept
{
    return row(partials, k, trail.lanes);
}

void add(std::span<Real> into, std::span<const Real> partials, const Trail &trail) noexcept
{
    for (std::size_t l = 0; l < trail.lanes; ++l)
    {
        const Real adjoint = trail.taken[l];
        into[l] = is_zero(adjoint) ? into[l] : into[l] + adjoint * partials[l];
    }
}

void spread(std::span<const std::uint32_t> operands,
            std::span<const Real> partials,
            const Trail &trail) noexcept
{
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        add(slot(trail, operands[k]), partial(partials, k, trail), trail);
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
        const std::span<Real> into = slot(trail, terms[k]);
        for (std::size_t l = 0; l < trail.lanes; ++l)
        {
            const Real adjoint = trail.taken[l];
            const Real scaled = adjoint * partials[l] * weights[k];
            into[l] = is_zero(adjoint) ? into[l] : into[l] + scaled;
        }
    }
}

void pass(std::span<Real> into, const Trail &trail) noexcept
{
    for (std::size_t l = 0; l < trail.lanes; ++l)
    {
        const Real adjoint = trail.taken[l];
        into[l] = is_zero(adjoint) ? into[l] : into[l] + adjoint;
    }
}

void choose(std::span<Real> into,
            std::size_t k,
            std::span<const Real> partials,
            const Trail &trail) noexcept
{
    for (std::size_t l = 0; l < trail.lanes; ++l)
    {
        const Real adjoint = trail.taken[l];
        if (is_zero(adjoint))
        {
            continue;
        }
        if (std::isnan(partials[l]))
        {
            into[l] += adjoint * partials[l];
        }
        else if (chosen(partials[l]) == k)
        {
            into[l] += adjoint;
        }
    }
}

void send(const Instruction &instruction,
          std::span<const Real> partials,
          const Trail &trail) noexcept
{
    const std::span<const std::uint32_t> operands = listed(instruction, trail.operands);
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        choose(slot(trail, operands[k]), k, partials, trail);
    }
}

void take(const Instruction &instruction, const Trail &trail) noexcept
{
    const std::span<Real> target = slot(trail, instruction.target);
    std::ranges::copy(target, trail.taken.begin());
    std::ranges::fill(target, 0.0);
}

void step(const Instruction &instruction,
          std::span<const Real> partials,
          const Trail &trail) noexcept
{
    take(instruction, trail);
    switch (instruction.op)
    {
    case Operator::Constant:
        return;
    case Operator::Variable:
        pass(row(trail.gradient, place(instruction.payload), trail.lanes), trail);
        return;
    case Operator::Sum:
        spread_sum(instruction, partials, trail);
        return;
    case Operator::Product:
        spread(listed(instruction, trail.operands), partials, trail);
        return;
    case Operator::Division:
    case Operator::Power:
    case Operator::Atan2:
        add(slot(trail, instruction.first), partial(partials, 0, trail), trail);
        add(slot(trail, instruction.second), partial(partials, 1, trail), trail);
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
        add(slot(trail, instruction.first), partials, trail);
        return;
    case Operator::Max:
    case Operator::Min:
        send(instruction, partials, trail);
        return;
    }
}

void sweep(const Block &block, Run run, const Trail &trail) noexcept
{
    for (std::uint32_t k = run.end; k > run.begin; --k)
    {
        const std::size_t start = trail.places[k - 1] * trail.lanes;
        const std::size_t n = (trail.places[k] - trail.places[k - 1]) * trail.lanes;
        step(block.instructions[k - 1], trail.partials.subspan(start, n), trail);
    }
}

void release(const Block &block, Run loads, Adjoints &adjoints) noexcept
{
    const std::size_t lanes = adjoints.lanes;
    for (std::uint32_t load = loads.begin; load < loads.end; ++load)
    {
        const std::span<Real> into =
            row(std::span(adjoints.shared), to_size(block.loads[load]), lanes);
        drain(row(std::span(adjoints.slots), load, lanes), into);
    }
}

}

void drain(std::span<Real> from, std::span<Real> into) noexcept
{
    for (std::size_t l = 0; l < from.size(); ++l)
    {
        into[l] += std::exchange(from[l], 0.0);
    }
}

Adjoints::Adjoints(const Tape &tape, std::size_t count)
    : lanes(lanes_of(tape, count)), slots(tape.width() * count, 0.0),
      shared(to_size(tape.pinned()) * count, 0.0), gradient(to_size(tape.variables()) * count, 0.0),
      taken(count, 0.0)
{
}

void retrace(const Block &block,
             std::size_t output,
             const Workspace &workspace,
             Adjoints &adjoints,
             std::span<Real> gradient) noexcept
{
    const Trail trail = trail_of(block, workspace, adjoints, gradient);
    for (const Run run : std::views::reverse(slice(std::span(block.runs), block.starts, output)))
    {
        sweep(block, run, trail);
    }
    for (const Run run : slice(std::span(block.reads), block.read_starts, output))
    {
        release(block, run, adjoints);
    }
}

void reverse(const Block &block,
             const Workspace &workspace,
             Adjoints &adjoints,
             std::span<Real> gradient) noexcept
{
    const Trail trail = trail_of(block, workspace, adjoints, gradient);
    const auto n = static_cast<std::uint32_t>(block.instructions.size());
    sweep(block, {.begin = 0, .end = n}, trail);
    const auto loads = static_cast<std::uint32_t>(block.loads.size());
    release(block, {.begin = 0, .end = loads}, adjoints);
}

}
