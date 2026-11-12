#include "expr/value.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "expr/frame.hpp"
#include "expr/node.hpp"
#include "expr/partials.hpp"
#include "expr/tape.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace biyos
{

namespace
{

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

double entropy(double value) noexcept
{
    if (std::fpclassify(value) == FP_ZERO)
    {
        return 0.0;
    }
    return value * std::log(value);
}

double sign_power(double value, double exponent) noexcept
{
    return std::copysign(std::pow(std::fabs(value), exponent), value);
}

double sum_of(const Instruction &instruction, const Frame &frame) noexcept
{
    const std::span<const std::uint32_t> terms = listed(instruction, frame.operands);
    const std::span<const double> weights =
        frame.weights.subspan(place(instruction.payload), terms.size() + 1);
    double total = weights[0];
    for (std::size_t k = 0; k < terms.size(); ++k)
    {
        total += weights[k + 1] * frame.slots[terms[k]];
    }
    return total;
}

double product_of(const Instruction &instruction, const Frame &frame) noexcept
{
    double total = constant_of(instruction);
    for (const std::uint32_t factor : listed(instruction, frame.operands))
    {
        total *= frame.slots[factor];
    }
    return total;
}

double first_largest(const Instruction &instruction, const Frame &frame) noexcept
{
    const std::span<const std::uint32_t> operands = listed(instruction, frame.operands);
    double best = frame.slots[operands[0]];
    for (const std::uint32_t operand : operands.subspan(1))
    {
        const double value = frame.slots[operand];
        if (value > best || std::isnan(value))
        {
            best = value;
        }
    }
    return best;
}

double first_smallest(const Instruction &instruction, const Frame &frame) noexcept
{
    const std::span<const std::uint32_t> operands = listed(instruction, frame.operands);
    double best = frame.slots[operands[0]];
    for (const std::uint32_t operand : operands.subspan(1))
    {
        const double value = frame.slots[operand];
        if (value < best || std::isnan(value))
        {
            best = value;
        }
    }
    return best;
}

double value(const Instruction &instruction, const Frame &frame) noexcept
{
    switch (instruction.op)
    {
    case Operator::Constant:
        return constant_of(instruction);
    case Operator::Variable:
        return frame.point[place(instruction.payload)];
    case Operator::Sum:
        return sum_of(instruction, frame);
    case Operator::Product:
        return product_of(instruction, frame);
    case Operator::Negation:
        return -left(instruction, frame);
    case Operator::Division:
        return left(instruction, frame) / right(instruction, frame);
    case Operator::ConstantPower:
        return std::pow(left(instruction, frame), constant_of(instruction));
    case Operator::Power:
        return std::pow(left(instruction, frame), right(instruction, frame));
    case Operator::Exp:
        return std::exp(left(instruction, frame));
    case Operator::Log:
        return std::log(left(instruction, frame));
    case Operator::Sqrt:
        return std::sqrt(left(instruction, frame));
    case Operator::Sin:
        return std::sin(left(instruction, frame));
    case Operator::Cos:
        return std::cos(left(instruction, frame));
    case Operator::Tan:
        return std::tan(left(instruction, frame));
    case Operator::Abs:
        return std::fabs(left(instruction, frame));
    case Operator::Max:
        return first_largest(instruction, frame);
    case Operator::Min:
        return first_smallest(instruction, frame);
    case Operator::Asin:
        return std::asin(left(instruction, frame));
    case Operator::Acos:
        return std::acos(left(instruction, frame));
    case Operator::Atan:
        return std::atan(left(instruction, frame));
    case Operator::Sinh:
        return std::sinh(left(instruction, frame));
    case Operator::Cosh:
        return std::cosh(left(instruction, frame));
    case Operator::Tanh:
        return std::tanh(left(instruction, frame));
    case Operator::Asinh:
        return std::asinh(left(instruction, frame));
    case Operator::Acosh:
        return std::acosh(left(instruction, frame));
    case Operator::Atanh:
        return std::atanh(left(instruction, frame));
    case Operator::Entropy:
        return entropy(left(instruction, frame));
    case Operator::SignPower:
        return sign_power(left(instruction, frame), constant_of(instruction));
    case Operator::Erf:
        return std::erf(left(instruction, frame));
    }
    return (std::numeric_limits<double>::quiet_NaN)();
}

Frame enter(const Block &block, std::span<const double> point, Workspace &workspace) noexcept
{
    const std::span<double> slots(workspace.slots);
    for (std::size_t k = 0; k < block.loads.size(); ++k)
    {
        slots[k] = workspace.shared[at(block.loads[k])];
    }
    return {
        .point = point,
        .operands = block.operands,
        .weights = block.weights,
        .slots = slots,
    };
}

[[noreturn]] void refuse(std::string object, std::string reason)
{
    throw Error(ErrorKind::InvalidArgument, std::move(object), std::move(reason));
}

}

Workspace::Workspace(const Tape &tape)
    : shared(at(tape.pinned()), 0.0), slots(tape.width(), 0.0), partials(at(tape.partials()), 0.0)
{
}

void check_sweep(const Tape &tape,
                 std::span<const double> point,
                 const Workspace &workspace,
                 std::span<const double> values)
{
    const auto given = static_cast<Offset>(point.size());
    if (given < tape.variables())
    {
        const std::string last = text_of(tape.variables() - 1);
        refuse("point", "size " + text_of(given) + " does not cover variable " + last);
    }
    const auto entries = static_cast<Offset>(values.size());
    if (entries != tape.roots())
    {
        refuse("values",
               "size " + text_of(entries) + ", not the root count " + text_of(tape.roots()));
    }
    const bool shared = static_cast<Offset>(workspace.shared.size()) >= tape.pinned();
    if (!shared || workspace.slots.size() < tape.width())
    {
        refuse("workspace", "sized for another tape");
    }
}

void forward(const Block &block, std::span<const double> point, Workspace &workspace) noexcept
{
    const Frame frame = enter(block, point, workspace);
    for (const Instruction &instruction : block.instructions)
    {
        frame.slots[instruction.target] = value(instruction, frame);
    }
}

void linearize(const Block &block, std::span<const double> point, Workspace &workspace) noexcept
{
    const Frame frame = enter(block, point, workspace);
    const std::span<double> partials = region(block, std::span(workspace.partials));
    for (std::size_t k = 0; k < block.instructions.size(); ++k)
    {
        const Instruction &instruction = block.instructions[k];
        const double result = value(instruction, frame);
        const std::uint32_t first = block.places[k];
        const std::uint32_t count = block.places[k + 1] - first;
        partials_of(instruction, frame, result, partials.subspan(first, count));
        frame.slots[instruction.target] = result;
    }
}

void store(const Block &block, std::span<const double> slots, std::span<double> outputs) noexcept
{
    const std::span<double> mine = outputs.subspan(at(block.first), block.outputs.size());
    for (std::size_t k = 0; k < mine.size(); ++k)
    {
        mine[k] = slots[block.outputs[k]];
    }
}

void forward(const Tape &tape,
             std::span<const double> point,
             Workspace &workspace,
             std::span<double> values)
{
    check_sweep(tape, point, workspace, values);
    const std::span<const Block> blocks = tape.blocks();
    const std::size_t prepass = at(tape.prepass());
    for (std::size_t k = 0; k < blocks.size(); ++k)
    {
        forward(blocks[k], point, workspace);
        const std::span<double> outputs = k < prepass ? std::span(workspace.shared) : values;
        store(blocks[k], workspace.slots, outputs);
    }
}

}
