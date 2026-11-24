#include "expr/derivatives/value.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text/text.hpp"
#include "expr/derivatives/partials.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/points.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
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

template <typename F> void map(const Instruction &instruction, const Frame &frame, F f) noexcept
{
    const std::span<const double> x = row(frame, instruction.first);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        frame.result[l] = f(x[l]);
    }
}

template <typename F> void combine(const Instruction &instruction, const Frame &frame, F f) noexcept
{
    const std::span<const double> x = row(frame, instruction.first);
    const std::span<const double> y = row(frame, instruction.second);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        frame.result[l] = f(x[l], y[l]);
    }
}

void load(const Instruction &instruction, const Frame &frame) noexcept
{
    const std::span<const double> x = row(frame.point, place(instruction.payload), frame.lanes);
    std::ranges::copy(x, frame.result.begin());
}

void sum_of(const Instruction &instruction, const Frame &frame) noexcept
{
    const std::span<const std::uint32_t> terms = listed(instruction, frame.operands);
    const std::span<const double> weights =
        frame.weights.subspan(place(instruction.payload), terms.size() + 1);
    std::ranges::fill(frame.result, weights[0]);
    for (std::size_t k = 0; k < terms.size(); ++k)
    {
        const std::span<const double> x = row(frame, terms[k]);
        for (std::size_t l = 0; l < frame.lanes; ++l)
        {
            frame.result[l] += weights[k + 1] * x[l];
        }
    }
}

void product_of(const Instruction &instruction, const Frame &frame) noexcept
{
    std::ranges::fill(frame.result, constant_of(instruction));
    for (const std::uint32_t factor : listed(instruction, frame.operands))
    {
        const std::span<const double> x = row(frame, factor);
        for (std::size_t l = 0; l < frame.lanes; ++l)
        {
            frame.result[l] *= x[l];
        }
    }
}

double larger(double best, double value) noexcept
{
    return value > best || std::isnan(value) ? value : best;
}

double smaller(double best, double value) noexcept
{
    return value < best || std::isnan(value) ? value : best;
}

template <typename F>
void extreme(const Instruction &instruction, const Frame &frame, F pick) noexcept
{
    const std::span<const std::uint32_t> operands = listed(instruction, frame.operands);
    std::ranges::copy(row(frame, operands[0]), frame.result.begin());
    for (const std::uint32_t operand : operands.subspan(1))
    {
        const std::span<const double> x = row(frame, operand);
        for (std::size_t l = 0; l < frame.lanes; ++l)
        {
            frame.result[l] = pick(frame.result[l], x[l]);
        }
    }
}

void fixed_power(const Instruction &instruction, const Frame &frame) noexcept
{
    const double exponent = constant_of(instruction);
    map(instruction, frame, [exponent](double x) { return std::pow(x, exponent); });
}

void signed_power(const Instruction &instruction, const Frame &frame) noexcept
{
    const double exponent = constant_of(instruction);
    map(instruction, frame, [exponent](double x) { return sign_power(x, exponent); });
}

void piecewise(const Instruction &instruction, const Frame &frame) noexcept
{
    const Pwl pwl = pwl_of(instruction, frame.weights);
    map(instruction, frame, [pwl](double x) { return value_at(pwl, x); });
}

Frame enter(const Block &block, std::span<const double> point, Workspace &workspace) noexcept
{
    const std::span<double> slots(workspace.slots);
    const std::span<const double> shared(workspace.shared);
    const std::size_t lanes = workspace.lanes;
    for (std::size_t k = 0; k < block.loads.size(); ++k)
    {
        std::ranges::copy(row(shared, at(block.loads[k]), lanes), row(slots, k, lanes).begin());
    }
    return {
        .point = point,
        .operands = block.operands,
        .weights = block.weights,
        .slots = slots,
        .result = workspace.result,
        .spare = workspace.spare,
        .lanes = lanes,
    };
}

[[noreturn]] void refuse(std::string object, std::string reason)
{
    throw Error(ErrorKind::InvalidArgument, std::move(object), std::move(reason));
}

void check_point(const Tape &tape, std::span<const double> point, std::size_t lanes)
{
    const std::string size = "size " + text_of(point.size());
    if (point.size() % lanes != 0)
    {
        refuse("point", size + " is not a multiple of " + text_of(lanes) + " lanes");
    }
    if (static_cast<Offset>(point.size() / lanes) < tape.variables())
    {
        const std::string last = text_of(tape.variables() - 1) + in_lanes(lanes);
        refuse("point", size + " does not cover variable " + last);
    }
}

void check_seeds(std::span<const Index> variables, std::size_t lanes, std::size_t size)
{
    if (variables.size() > lanes)
    {
        refuse("variables", text_of(variables.size()) + " for " + text_of(lanes) + " lanes");
    }
    if (size % lanes != 0)
    {
        refuse("directions",
               "size " + text_of(size) + " is not a multiple of " + text_of(lanes) + " lanes");
    }
    for (const Index variable : variables)
    {
        if (variable < 0 || static_cast<std::size_t>(variable) >= size / lanes)
        {
            refuse("variables", "variable " + text_of(variable) + " outside the directions");
        }
    }
}

}

void evaluate(const Instruction &instruction, const Frame &frame) noexcept
{
    switch (instruction.op)
    {
    case Operator::Constant:
        std::ranges::fill(frame.result, constant_of(instruction));
        return;
    case Operator::Variable:
        load(instruction, frame);
        return;
    case Operator::Sum:
        sum_of(instruction, frame);
        return;
    case Operator::Product:
        product_of(instruction, frame);
        return;
    case Operator::Negation:
        map(instruction, frame, [](double x) { return -x; });
        return;
    case Operator::Division:
        combine(instruction, frame, [](double x, double y) { return x / y; });
        return;
    case Operator::ConstantPower:
        fixed_power(instruction, frame);
        return;
    case Operator::Power:
        combine(instruction, frame, [](double x, double y) { return std::pow(x, y); });
        return;
    case Operator::Exp:
        map(instruction, frame, [](double x) { return std::exp(x); });
        return;
    case Operator::Log:
        map(instruction, frame, [](double x) { return std::log(x); });
        return;
    case Operator::Sqrt:
        map(instruction, frame, [](double x) { return std::sqrt(x); });
        return;
    case Operator::Sin:
        map(instruction, frame, [](double x) { return std::sin(x); });
        return;
    case Operator::Cos:
        map(instruction, frame, [](double x) { return std::cos(x); });
        return;
    case Operator::Tan:
        map(instruction, frame, [](double x) { return std::tan(x); });
        return;
    case Operator::Abs:
        map(instruction, frame, [](double x) { return std::fabs(x); });
        return;
    case Operator::Max:
        extreme(instruction, frame, larger);
        return;
    case Operator::Min:
        extreme(instruction, frame, smaller);
        return;
    case Operator::Asin:
        map(instruction, frame, [](double x) { return std::asin(x); });
        return;
    case Operator::Acos:
        map(instruction, frame, [](double x) { return std::acos(x); });
        return;
    case Operator::Atan:
        map(instruction, frame, [](double x) { return std::atan(x); });
        return;
    case Operator::Sinh:
        map(instruction, frame, [](double x) { return std::sinh(x); });
        return;
    case Operator::Cosh:
        map(instruction, frame, [](double x) { return std::cosh(x); });
        return;
    case Operator::Tanh:
        map(instruction, frame, [](double x) { return std::tanh(x); });
        return;
    case Operator::Asinh:
        map(instruction, frame, [](double x) { return std::asinh(x); });
        return;
    case Operator::Acosh:
        map(instruction, frame, [](double x) { return std::acosh(x); });
        return;
    case Operator::Atanh:
        map(instruction, frame, [](double x) { return std::atanh(x); });
        return;
    case Operator::Entropy:
        map(instruction, frame, entropy);
        return;
    case Operator::SignPower:
        signed_power(instruction, frame);
        return;
    case Operator::Erf:
        map(instruction, frame, [](double x) { return std::erf(x); });
        return;
    case Operator::PiecewiseLinear:
        piecewise(instruction, frame);
        return;
    }
}

void keep(const Instruction &instruction, const Frame &frame) noexcept
{
    std::ranges::copy(frame.result, row(frame.slots, instruction.target, frame.lanes).begin());
}

Workspace::Workspace(const Tape &tape, std::size_t count)
    : lanes(lanes_of(tape, count)), shared(at(tape.pinned()) * count, 0.0),
      slots(tape.width() * count, 0.0), partials(at(tape.partials()) * count, 0.0),
      result(count, 0.0), spare(count, 0.0)
{
}

std::string in_lanes(std::size_t lanes)
{
    return lanes == 1 ? "" : " in " + text_of(lanes) + " lanes";
}

std::size_t lanes_of(std::size_t count)
{
    if (count == 0)
    {
        refuse("lanes", "zero");
    }
    return count;
}

std::size_t sized(std::size_t size, std::size_t lanes)
{
    if (size != 0 && lanes > (std::numeric_limits<std::size_t>::max)() / size)
    {
        refuse("lanes", text_of(lanes) + " times size " + text_of(size) + " overflows");
    }
    return size * lanes;
}

std::size_t lanes_of(const Tape &tape, std::size_t count)
{
    lanes_of(count);
    sized(at(tape.roots()), count);
    sized(at(tape.variables()), count);
    sized(at(tape.pinned()), count);
    sized(at(tape.partials()), count);
    sized(tape.width(), count);
    return count;
}

void check_sweep(const Tape &tape,
                 std::span<const double> point,
                 const Workspace &workspace,
                 std::span<const double> values)
{
    const std::size_t lanes = lanes_of(tape, workspace.lanes);
    check_point(tape, point, lanes);
    if (values.size() != at(tape.roots()) * lanes)
    {
        const std::string roots = text_of(tape.roots()) + in_lanes(lanes);
        refuse("values", "size " + text_of(values.size()) + ", not the root count " + roots);
    }
    const bool shared = workspace.shared.size() >= at(tape.pinned()) * lanes;
    const bool slots = workspace.slots.size() >= tape.width() * lanes;
    const bool rows = workspace.result.size() >= lanes && workspace.spare.size() >= lanes;
    if (!shared || !slots || !rows)
    {
        refuse("workspace", "sized for another tape");
    }
}

void forward(const Block &block, std::span<const double> point, Workspace &workspace) noexcept
{
    const Frame frame = enter(block, point, workspace);
    for (const Instruction &instruction : block.instructions)
    {
        evaluate(instruction, frame);
        keep(instruction, frame);
    }
}

void linearize(const Block &block, std::span<const double> point, Workspace &workspace) noexcept
{
    const Frame frame = enter(block, point, workspace);
    const std::size_t lanes = workspace.lanes;
    const std::span<double> partials = region(block, std::span(workspace.partials), lanes);
    for (std::size_t k = 0; k < block.instructions.size(); ++k)
    {
        const Instruction &instruction = block.instructions[k];
        evaluate(instruction, frame);
        const std::size_t start = block.places[k] * lanes;
        const std::size_t n = (block.places[k + 1] - block.places[k]) * lanes;
        partials_of(instruction, frame, partials.subspan(start, n));
        keep(instruction, frame);
    }
}

void store(const Block &block, const Workspace &workspace, std::span<double> outputs) noexcept
{
    const std::span<const double> slots(workspace.slots);
    const std::size_t lanes = workspace.lanes;
    for (std::size_t k = 0; k < block.outputs.size(); ++k)
    {
        const std::span<double> mine = row(outputs, at(block.begin) + k, lanes);
        std::ranges::copy(row(slots, block.outputs[k], lanes), mine.begin());
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
        store(blocks[k], workspace, outputs);
    }
}

void broadcast(std::span<const double> point, std::size_t lanes, std::span<double> points)
{
    if (points.size() != sized(point.size(), lanes_of(lanes)))
    {
        const std::string size = "size " + text_of(points.size());
        refuse("points", size + ", not the point size " + text_of(point.size()) + in_lanes(lanes));
    }
    for (std::size_t k = 0; k < point.size(); ++k)
    {
        std::ranges::fill(row(points, k, lanes), point[k]);
    }
}

void seed_directions(std::span<const Index> variables,
                     std::size_t lanes,
                     std::span<double> directions)
{
    check_seeds(variables, lanes_of(lanes), directions.size());
    std::ranges::fill(directions, 0.0);
    for (std::size_t l = 0; l < variables.size(); ++l)
    {
        directions[static_cast<std::size_t>(variables[l]) * lanes + l] = 1.0;
    }
}

}
