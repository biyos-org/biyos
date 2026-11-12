#include "expr/reverse.hpp"

#include "core/index.hpp"
#include "expr/frame.hpp"
#include "expr/node.hpp"
#include "expr/partials.hpp"
#include "expr/tape.hpp"
#include "expr/value.hpp"

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
    std::span<const double> weights;
    std::span<const std::uint32_t> places;
    std::span<const double> partials;
    std::span<double> slots;
    std::span<double> gradient;
};

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

template <typename T>
std::span<const T> part(const std::vector<T> &items,
                        const std::vector<Offset> &starts,
                        std::size_t k) noexcept
{
    return std::span(items).subspan(at(starts[k]), at(starts[k + 1] - starts[k]));
}

Trail trail_of(const Block &block,
               const Workspace &workspace,
               Adjoints &adjoints,
               std::span<double> gradient) noexcept
{
    return {
        .operands = block.operands,
        .weights = block.weights,
        .places = block.places,
        .partials = region(block, std::span(workspace.partials)),
        .slots = adjoints.slots,
        .gradient = gradient,
    };
}

void spread(std::span<const std::uint32_t> operands,
            std::span<const double> partials,
            double adjoint,
            std::span<double> slots) noexcept
{
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        slots[operands[k]] += adjoint * partials[k];
    }
}

void spread_sum(const Instruction &instruction,
                std::span<const double> partials,
                const Trail &trail,
                double adjoint) noexcept
{
    const std::span<const std::uint32_t> terms = listed(instruction, trail.operands);
    const std::span<const double> weights =
        trail.weights.subspan(place(instruction.payload) + 1, terms.size());
    spread(terms, weights, adjoint * partials[0], trail.slots);
}

void send(const Instruction &instruction,
          std::span<const double> partials,
          const Trail &trail,
          double adjoint) noexcept
{
    const std::span<const std::uint32_t> operands = listed(instruction, trail.operands);
    if (std::isnan(partials[0]))
    {
        for (const std::uint32_t operand : operands)
        {
            trail.slots[operand] += adjoint * partials[0];
        }
        return;
    }
    trail.slots[operands[chosen(partials)]] += adjoint;
}

void step(const Instruction &instruction,
          std::span<const double> partials,
          const Trail &trail) noexcept
{
    const double adjoint = std::exchange(trail.slots[instruction.target], 0.0);
    if (std::fpclassify(adjoint) == FP_ZERO)
    {
        return;
    }
    switch (instruction.op)
    {
    case Operator::Constant:
        return;
    case Operator::Variable:
        trail.gradient[place(instruction.payload)] += adjoint;
        return;
    case Operator::Sum:
        spread_sum(instruction, partials, trail, adjoint);
        return;
    case Operator::Product:
        spread(listed(instruction, trail.operands), partials, adjoint, trail.slots);
        return;
    case Operator::Division:
    case Operator::Power:
        trail.slots[instruction.first] += adjoint * partials[0];
        trail.slots[instruction.second] += adjoint * partials[1];
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
        trail.slots[instruction.first] += adjoint * partials[0];
        return;
    case Operator::Max:
    case Operator::Min:
        send(instruction, partials, trail, adjoint);
        return;
    }
}

void sweep(const Block &block, Run run, const Trail &trail) noexcept
{
    for (std::uint32_t k = run.end; k > run.begin; --k)
    {
        const std::uint32_t first = trail.places[k - 1];
        const std::span<const double> partials =
            trail.partials.subspan(first, trail.places[k] - first);
        step(block.instructions[k - 1], partials, trail);
    }
}

void release(const Block &block, Run loads, Adjoints &adjoints) noexcept
{
    for (std::uint32_t load = loads.begin; load < loads.end; ++load)
    {
        adjoints.shared[at(block.loads[load])] += std::exchange(adjoints.slots[load], 0.0);
    }
}
}

Adjoints::Adjoints(const Tape &tape)
    : slots(tape.width(), 0.0), shared(at(tape.pinned()), 0.0), gradient(at(tape.variables()), 0.0)
{
}

void retrace(const Block &block,
             std::size_t output,
             const Workspace &workspace,
             Adjoints &adjoints,
             std::span<double> gradient) noexcept
{
    const Trail trail = trail_of(block, workspace, adjoints, gradient);
    for (const Run run : std::views::reverse(part(block.runs, block.starts, output)))
    {
        sweep(block, run, trail);
    }
    for (const Run run : part(block.reads, block.read_starts, output))
    {
        release(block, run, adjoints);
    }
}

void reverse(const Block &block,
             const Workspace &workspace,
             Adjoints &adjoints,
             std::span<double> gradient) noexcept
{
    const Trail trail = trail_of(block, workspace, adjoints, gradient);
    const auto count = static_cast<std::uint32_t>(block.instructions.size());
    sweep(block, {.begin = 0, .end = count}, trail);
    const auto loads = static_cast<std::uint32_t>(block.loads.size());
    release(block, {.begin = 0, .end = loads}, adjoints);
}

}
