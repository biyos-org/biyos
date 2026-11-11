#include "expr/value.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "expr/node.hpp"
#include "expr/tape.hpp"

#include <bit>
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

struct Frame
{
    std::span<const double> point;
    std::span<const std::uint32_t> operands;
    std::span<const double> weights;
    std::span<double> slots;
};

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

std::size_t place(std::uint64_t payload) noexcept
{
    return static_cast<std::size_t>(payload);
}

double constant_of(const Instruction &instruction) noexcept
{
    return std::bit_cast<double>(instruction.payload);
}

double left(const Instruction &instruction, const Frame &frame) noexcept
{
    return frame.slots[instruction.first];
}

double right(const Instruction &instruction, const Frame &frame) noexcept
{
    return frame.slots[instruction.second];
}

std::span<const std::uint32_t> listed(const Instruction &instruction, const Frame &frame) noexcept
{
    return frame.operands.subspan(instruction.first, instruction.second - instruction.first);
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
    const std::span<const std::uint32_t> terms = listed(instruction, frame);
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
    for (const std::uint32_t factor : listed(instruction, frame))
    {
        total *= frame.slots[factor];
    }
    return total;
}

double first_largest(const Instruction &instruction, const Frame &frame) noexcept
{
    const std::span<const std::uint32_t> operands = listed(instruction, frame);
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
    const std::span<const std::uint32_t> operands = listed(instruction, frame);
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

void run(const Block &block, std::span<const double> point, Workspace &workspace) noexcept
{
    const std::span<double> slots(workspace.slots);
    for (std::size_t k = 0; k < block.loads.size(); ++k)
    {
        slots[k] = workspace.shared[at(block.loads[k])];
    }
    const Frame frame{
        .point = point,
        .operands = block.operands,
        .weights = block.weights,
        .slots = slots,
    };
    for (const Instruction &instruction : block.instructions)
    {
        slots[instruction.target] = value(instruction, frame);
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

[[noreturn]] void refuse(std::string object, std::string reason)
{
    throw Error(ErrorKind::InvalidArgument, std::move(object), std::move(reason));
}

void check(const Tape &tape,
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

}

Workspace::Workspace(const Tape &tape) : shared(at(tape.pinned()), 0.0), slots(tape.width(), 0.0) {}

void forward(const Tape &tape,
             std::span<const double> point,
             Workspace &workspace,
             std::span<double> values)
{
    check(tape, point, workspace, values);
    const std::span<const Block> blocks = tape.blocks();
    const std::size_t prepass = at(tape.prepass());
    for (std::size_t k = 0; k < blocks.size(); ++k)
    {
        run(blocks[k], point, workspace);
        const std::span<double> outputs = k < prepass ? std::span(workspace.shared) : values;
        store(blocks[k], workspace.slots, outputs);
    }
}

}
