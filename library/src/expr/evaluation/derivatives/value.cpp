#include "expr/evaluation/derivatives/value.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/text/text.hpp"
#include "expr/evaluation/derivatives/partials.hpp"
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

Real entropy(Real value) noexcept
{
    if (std::fpclassify(value) == FP_ZERO)
    {
        return 0.0;
    }
    return value * std::log(value);
}

Real sign_power(Real value, Real exponent) noexcept
{
    return std::copysign(std::pow(std::fabs(value), exponent), value);
}

Real angle(Real y, Real x) noexcept
{
    return std::atan2(y + 0.0, x + 0.0);
}

Real truth(bool holds, Real a, Real b) noexcept
{
    if (std::isnan(a) || std::isnan(b))
    {
        return (std::numeric_limits<Real>::quiet_NaN)();
    }
    return holds ? 1.0 : 0.0;
}

bool less_equal(const Tolerances &tolerances, Real a, Real b) noexcept
{
    return tolerances.at_most(a, b);
}

bool greater_equal(const Tolerances &tolerances, Real a, Real b) noexcept
{
    return tolerances.at_most(b, a);
}

bool equal(const Tolerances &tolerances, Real a, Real b) noexcept
{
    return tolerances.approx_equal(a, b);
}

bool less(const Tolerances &tolerances, Real a, Real b) noexcept
{
    return !greater_equal(tolerances, a, b);
}

bool greater(const Tolerances &tolerances, Real a, Real b) noexcept
{
    return !less_equal(tolerances, a, b);
}

bool not_equal(const Tolerances &tolerances, Real a, Real b) noexcept
{
    return !equal(tolerances, a, b);
}

template <typename F> void map(const Instruction &instruction, const Frame &frame, F f) noexcept
{
    const std::span<const Real> x = row(frame, instruction.first);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        frame.result[l] = f(x[l]);
    }
}

template <typename F> void combine(const Instruction &instruction, const Frame &frame, F f) noexcept
{
    const std::span<const Real> x = row(frame, instruction.first);
    const std::span<const Real> y = row(frame, instruction.second);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        frame.result[l] = f(x[l], y[l]);
    }
}

template <typename F>
void compare(const Instruction &instruction, const Frame &frame, F holds) noexcept
{
    const std::span<const Real> x = row(frame, instruction.first);
    const std::span<const Real> y = row(frame, instruction.second);
    for (std::size_t l = 0; l < frame.lanes; ++l)
    {
        frame.result[l] = truth(holds(frame.tolerances, x[l], y[l]), x[l], y[l]);
    }
}

void load(const Instruction &instruction, const Frame &frame) noexcept
{
    const std::span<const Real> x = row(frame.point, place(instruction.payload), frame.lanes);
    std::ranges::copy(x, frame.result.begin());
}

void sum_of(const Instruction &instruction, const Frame &frame) noexcept
{
    const std::span<const std::uint32_t> terms = listed(instruction, frame.operands);
    const std::span<const Real> weights =
        frame.weights.subspan(place(instruction.payload), terms.size() + 1);
    std::ranges::fill(frame.result, weights[0]);
    for (std::size_t k = 0; k < terms.size(); ++k)
    {
        const std::span<const Real> x = row(frame, terms[k]);
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
        const std::span<const Real> x = row(frame, factor);
        for (std::size_t l = 0; l < frame.lanes; ++l)
        {
            frame.result[l] *= x[l];
        }
    }
}

Real larger(Real best, Real value) noexcept
{
    return value > best || std::isnan(value) ? value : best;
}

Real smaller(Real best, Real value) noexcept
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
        const std::span<const Real> x = row(frame, operand);
        for (std::size_t l = 0; l < frame.lanes; ++l)
        {
            frame.result[l] = pick(frame.result[l], x[l]);
        }
    }
}

void fixed_power(const Instruction &instruction, const Frame &frame) noexcept
{
    const Real exponent = constant_of(instruction);
    map(instruction, frame, [exponent](Real x) { return std::pow(x, exponent); });
}

void signed_power(const Instruction &instruction, const Frame &frame) noexcept
{
    const Real exponent = constant_of(instruction);
    map(instruction, frame, [exponent](Real x) { return sign_power(x, exponent); });
}

void piecewise(const Instruction &instruction, const Frame &frame) noexcept
{
    const PiecewiseLinear pwl = piecewise_linear_of(instruction, frame.weights);
    map(instruction, frame, [pwl](Real x) { return value_at(pwl, x); });
}

Frame enter(const Block &block,
            std::span<const Real> point,
            std::span<const Real> shared,
            const Tolerances &tolerances,
            Workspace &workspace) noexcept
{
    const std::span<Real> slots(workspace.slots);
    const std::size_t lanes = workspace.lanes;
    for (std::size_t k = 0; k < block.loads.size(); ++k)
    {
        std::ranges::copy(row(shared, to_size(block.loads[k]), lanes),
                          row(slots, k, lanes).begin());
    }
    return {
        .point = point,
        .operands = block.operands,
        .weights = block.weights,
        .slots = slots,
        .result = workspace.result,
        .spare = workspace.spare,
        .lanes = lanes,
        .tolerances = tolerances,
    };
}

[[noreturn]] void refuse(std::string object, std::string reason)
{
    throw Error(ErrorKind::InvalidArgument, std::move(object), std::move(reason));
}

void check_point(const Tape &tape, std::span<const Real> point, std::size_t lanes)
{
    const std::string size = "size " + text_of(point.size());
    if (point.size() % lanes != 0)
    {
        refuse("point", size + " is not a multiple of " + text_of(lanes) + " lanes");
    }
    if (to_offset(point.size() / lanes) < tape.variables())
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
        if (variable < 0 || to_size(variable) >= size / lanes)
        {
            refuse("variables", "variable " + text_of(variable) + " outside the directions");
        }
    }
}

void repeat(std::span<const Real> point, std::size_t lanes, std::span<Real> points) noexcept
{
    for (std::size_t k = 0; k < point.size(); ++k)
    {
        std::ranges::fill(row(points, k, lanes), point[k]);
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
        map(instruction, frame, [](Real x) { return -x; });
        return;
    case Operator::Division:
        combine(instruction, frame, [](Real x, Real y) { return x / y; });
        return;
    case Operator::ConstantPower:
        fixed_power(instruction, frame);
        return;
    case Operator::Power:
        combine(instruction, frame, [](Real x, Real y) { return std::pow(x, y); });
        return;
    case Operator::Exp:
        map(instruction, frame, [](Real x) { return std::exp(x); });
        return;
    case Operator::Log:
        map(instruction, frame, [](Real x) { return std::log(x); });
        return;
    case Operator::Sqrt:
        map(instruction, frame, [](Real x) { return std::sqrt(x); });
        return;
    case Operator::Sin:
        map(instruction, frame, [](Real x) { return std::sin(x); });
        return;
    case Operator::Cos:
        map(instruction, frame, [](Real x) { return std::cos(x); });
        return;
    case Operator::Tan:
        map(instruction, frame, [](Real x) { return std::tan(x); });
        return;
    case Operator::Abs:
        map(instruction, frame, [](Real x) { return std::fabs(x); });
        return;
    case Operator::Max:
        extreme(instruction, frame, larger);
        return;
    case Operator::Min:
        extreme(instruction, frame, smaller);
        return;
    case Operator::Asin:
        map(instruction, frame, [](Real x) { return std::asin(x); });
        return;
    case Operator::Acos:
        map(instruction, frame, [](Real x) { return std::acos(x); });
        return;
    case Operator::Atan:
        map(instruction, frame, [](Real x) { return std::atan(x); });
        return;
    case Operator::Sinh:
        map(instruction, frame, [](Real x) { return std::sinh(x); });
        return;
    case Operator::Cosh:
        map(instruction, frame, [](Real x) { return std::cosh(x); });
        return;
    case Operator::Tanh:
        map(instruction, frame, [](Real x) { return std::tanh(x); });
        return;
    case Operator::Asinh:
        map(instruction, frame, [](Real x) { return std::asinh(x); });
        return;
    case Operator::Acosh:
        map(instruction, frame, [](Real x) { return std::acosh(x); });
        return;
    case Operator::Atanh:
        map(instruction, frame, [](Real x) { return std::atanh(x); });
        return;
    case Operator::Entropy:
        map(instruction, frame, entropy);
        return;
    case Operator::SignPower:
        signed_power(instruction, frame);
        return;
    case Operator::Erf:
        map(instruction, frame, [](Real x) { return std::erf(x); });
        return;
    case Operator::PiecewiseLinear:
        piecewise(instruction, frame);
        return;
    case Operator::Atan2:
        combine(instruction, frame, angle);
        return;
    case Operator::LessEqual:
        compare(instruction, frame, less_equal);
        return;
    case Operator::GreaterEqual:
        compare(instruction, frame, greater_equal);
        return;
    case Operator::Equal:
        compare(instruction, frame, equal);
        return;
    case Operator::Less:
        compare(instruction, frame, less);
        return;
    case Operator::Greater:
        compare(instruction, frame, greater);
        return;
    case Operator::NotEqual:
        compare(instruction, frame, not_equal);
        return;
    }
}

void keep(const Instruction &instruction, const Frame &frame) noexcept
{
    std::ranges::copy(frame.result, row(frame.slots, instruction.target, frame.lanes).begin());
}

Workspace::Workspace(const Tape &tape, std::size_t count)
    : lanes(lanes_of(tape, count)), shared(to_size(tape.pinned()) * count, 0.0),
      slots(tape.width() * count, 0.0), partials(to_size(tape.partials()) * count, 0.0),
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
    sized(to_size(tape.roots()), count);
    sized(to_size(tape.variables()), count);
    sized(to_size(tape.pinned()), count);
    sized(to_size(tape.partials()), count);
    sized(tape.width(), count);
    return count;
}

void check_sweep(const Tape &tape,
                 std::span<const Real> point,
                 const Workspace &workspace,
                 std::span<const Real> values)
{
    const std::size_t lanes = lanes_of(tape, workspace.lanes);
    check_point(tape, point, lanes);
    if (values.size() != to_size(tape.roots()) * lanes)
    {
        const std::string roots = text_of(tape.roots()) + in_lanes(lanes);
        refuse("values", "size " + text_of(values.size()) + ", not the root count " + roots);
    }
    const bool shared = workspace.shared.size() >= to_size(tape.pinned()) * lanes;
    const bool slots = workspace.slots.size() >= tape.width() * lanes;
    const bool rows = workspace.result.size() >= lanes && workspace.spare.size() >= lanes;
    if (!shared || !slots || !rows)
    {
        refuse("workspace", "sized for another tape");
    }
}

void forward(const Block &block,
             std::span<const Real> point,
             const Tolerances &tolerances,
             Workspace &workspace) noexcept
{
    forward(block, point, workspace.shared, tolerances, workspace);
}

void forward(const Block &block,
             std::span<const Real> point,
             std::span<const Real> shared,
             const Tolerances &tolerances,
             Workspace &workspace) noexcept
{
    const Frame frame = enter(block, point, shared, tolerances, workspace);
    for (const Instruction &instruction : block.instructions)
    {
        evaluate(instruction, frame);
        keep(instruction, frame);
    }
}

void linearize(const Block &block,
               std::span<const Real> point,
               const Tolerances &tolerances,
               Workspace &workspace) noexcept
{
    linearize(block, point, workspace.shared, tolerances, workspace.partials, workspace);
}

void linearize(const Block &block,
               std::span<const Real> point,
               std::span<const Real> shared,
               const Tolerances &tolerances,
               std::span<Real> all,
               Workspace &workspace) noexcept
{
    const Frame frame = enter(block, point, shared, tolerances, workspace);
    const std::size_t lanes = workspace.lanes;
    const std::span<Real> partials = region(block, all, lanes);
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

void store(const Block &block, const Workspace &workspace, std::span<Real> outputs) noexcept
{
    const std::span<const Real> slots(workspace.slots);
    const std::size_t lanes = workspace.lanes;
    for (std::size_t k = 0; k < block.outputs.size(); ++k)
    {
        const std::span<Real> mine = row(outputs, to_size(block.begin) + k, lanes);
        std::ranges::copy(row(slots, block.outputs[k], lanes), mine.begin());
    }
}

void forward(const Tape &tape,
             std::span<const Real> point,
             const Tolerances &tolerances,
             Workspace &workspace,
             std::span<Real> values)
{
    check_sweep(tape, point, workspace, values);
    const std::span<const Block> blocks = tape.blocks();
    const std::size_t prepass = to_size(tape.prepass());
    for (std::size_t k = 0; k < blocks.size(); ++k)
    {
        forward(blocks[k], point, tolerances, workspace);
        const std::span<Real> outputs = k < prepass ? std::span(workspace.shared) : values;
        store(blocks[k], workspace, outputs);
    }
}

void broadcast(std::span<const Real> point, std::size_t lanes, std::span<Real> points)
{
    if (points.size() != sized(point.size(), lanes_of(lanes)))
    {
        const std::string size = "size " + text_of(points.size());
        refuse("points", size + ", not the point size " + text_of(point.size()) + in_lanes(lanes));
    }
    repeat(point, lanes, points);
}

void seed_directions(std::span<const Index> variables,
                     std::size_t lanes,
                     std::span<Real> directions)
{
    check_seeds(variables, lanes_of(lanes), directions.size());
    std::ranges::fill(directions, 0.0);
    for (std::size_t l = 0; l < variables.size(); ++l)
    {
        directions[to_size(variables[l]) * lanes + l] = 1.0;
    }
}

}
