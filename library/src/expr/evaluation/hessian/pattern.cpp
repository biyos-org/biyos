#include "expr/evaluation/hessian/pattern.hpp"

#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/evaluation/hessian/tangents.hpp"
#include "expr/graph/node.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/layout.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

void merge(std::vector<std::uint32_t> &row)
{
    std::ranges::sort(row);
    row.erase(std::ranges::unique(row).begin(), row.end());
}

bool before(Entry a, Entry b) noexcept
{
    return a.column != b.column ? a.column < b.column : a.row < b.row;
}

}

Pusher::Pusher(const Tape &tape)
    : tape_(tape), local_(to_size(tape.variables()), 0), occupants_(tape.width(), 0),
      held_(to_size(tape.pinned()), 0)
{
}

Shape Pusher::shape(Offset element)
{
    start(element);
    rise(element);
    const Block &block = block_of(tape_.blocks().subspan(to_size(tape_.prepass())), element);
    walk(block, to_size(element - block.begin));
    eliminate();
    return {.entries = entries(), .pushes = pushes_};
}

Pusher::Rule Pusher::rule_of(const Instruction &instruction) noexcept
{
    switch (instruction.op)
    {
    case Operator::Constant:
        return {.form = Form::Constant, .bending = Bending::None};
    case Operator::Variable:
        return {.form = Form::Variable, .bending = Bending::None};
    case Operator::Sum:
    case Operator::Max:
    case Operator::Min:
        return {.form = Form::Listed, .bending = Bending::None};
    case Operator::Product:
        return {.form = Form::Listed, .bending = Bending::Cross};
    case Operator::Negation:
    case Operator::Abs:
    case Operator::PiecewiseLinear:
        return {.form = Form::Unary, .bending = Bending::None};
    case Operator::ConstantPower:
    case Operator::SignPower:
        return {.form = Form::Unary,
                .bending = curved(constant_of(instruction)) ? Bending::Full : Bending::None};
    case Operator::Division:
        return {.form = Form::Binary, .bending = Bending::Quotient};
    case Operator::Power:
    case Operator::Atan2:
        return {.form = Form::Binary, .bending = Bending::Full};
    case Operator::Exp:
    case Operator::Log:
    case Operator::Sqrt:
    case Operator::Sin:
    case Operator::Cos:
    case Operator::Tan:
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
    case Operator::Erf:
        return {.form = Form::Unary, .bending = Bending::Full};
    }
    return {.form = Form::Constant, .bending = Bending::None};
}

void Pusher::start(Offset element)
{
    const std::span<const Index> variables = variables_of(tape_.pattern(), element);
    variables_ = narrow(variables.size());
    for (std::size_t k = 0; k < variables.size(); ++k)
    {
        local_[to_size(variables[k])] = narrow(k);
    }
    nodes_.clear();
    operands_.clear();
    pushes_ = 0;
}

void Pusher::rise(Offset element)
{
    const std::span<const Block> prepass = tape_.blocks().first(to_size(tape_.prepass()));
    for (const Range range : tape_.route(element))
    {
        for (Offset position = range.begin; position < range.end; ++position)
        {
            const Block &block = block_of(prepass, position);
            const std::size_t output = to_size(position - block.begin);
            walk(block, output);
            held_[to_size(position)] = occupants_[block.outputs[output]];
        }
    }
}

void Pusher::walk(const Block &block, std::size_t output)
{
    for (const Run run : slice(std::span(block.reads), block.read_starts, output))
    {
        for (std::uint32_t k = run.begin; k < run.end; ++k)
        {
            occupants_[k] = held_[to_size(block.loads[k])];
        }
    }
    for (const Run run : slice(std::span(block.runs), block.starts, output))
    {
        for (std::uint32_t k = run.begin; k < run.end; ++k)
        {
            enter(block.instructions[k], block.operands);
        }
    }
}

void Pusher::enter(const Instruction &instruction, std::span<const std::uint32_t> operands)
{
    const Rule rule = rule_of(instruction);
    if (rule.form == Form::Variable)
    {
        occupants_[instruction.target] = local_[place(instruction.payload)];
        return;
    }
    const std::uint32_t begin = narrow(operands_.size());
    gather(instruction, rule.form, operands);
    nodes_.push_back({.bending = rule.bending, .begin = begin, .end = narrow(operands_.size())});
    occupants_[instruction.target] = variables_ + narrow(nodes_.size() - 1);
}

void Pusher::gather(const Instruction &instruction,
                    Form form,
                    std::span<const std::uint32_t> operands)
{
    switch (form)
    {
    case Form::Constant:
    case Form::Variable:
        return;
    case Form::Unary:
        operands_.push_back(occupants_[instruction.first]);
        return;
    case Form::Binary:
        operands_.push_back(occupants_[instruction.first]);
        operands_.push_back(occupants_[instruction.second]);
        return;
    case Form::Listed:
        for (const std::uint32_t slot : listed(instruction, operands))
        {
            operands_.push_back(occupants_[slot]);
        }
        return;
    }
}

void Pusher::eliminate()
{
    const std::size_t ids = variables_ + nodes_.size();
    rows_.resize((std::max)(rows_.size(), ids));
    for (std::vector<std::uint32_t> &row : std::span(rows_).first(ids))
    {
        row.clear();
    }
    for (std::size_t n = nodes_.size(); n > 0; --n)
    {
        const Node &node = nodes_[n - 1];
        const std::span<const std::uint32_t> operands =
            std::span(operands_).subspan(node.begin, node.end - node.begin);
        push(variables_ + narrow(n - 1), operands);
        create(node, operands);
    }
}

void Pusher::push(std::uint32_t id, std::span<const std::uint32_t> operands)
{
    std::vector<std::uint32_t> &row = rows_[id];
    merge(row);
    for (const std::uint32_t neighbour : row)
    {
        if (neighbour == id)
        {
            pairs(operands);
            continue;
        }
        for (const std::uint32_t operand : operands)
        {
            link(operand, neighbour);
        }
    }
}

void Pusher::create(const Node &node, std::span<const std::uint32_t> operands)
{
    switch (node.bending)
    {
    case Bending::None:
        return;
    case Bending::Full:
        pairs(operands);
        return;
    case Bending::Quotient:
        link(operands.front(), operands.back());
        link(operands.back(), operands.back());
        return;
    case Bending::Cross:
        for (std::size_t k = 0; k < operands.size(); ++k)
        {
            for (std::size_t m = k + 1; m < operands.size(); ++m)
            {
                link(operands[k], operands[m]);
            }
        }
        return;
    }
}

void Pusher::pairs(std::span<const std::uint32_t> operands)
{
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        for (std::size_t m = k; m < operands.size(); ++m)
        {
            link(operands[k], operands[m]);
        }
    }
}

void Pusher::link(std::uint32_t a, std::uint32_t b)
{
    rows_[(std::max)(a, b)].push_back((std::min)(a, b));
    ++pushes_;
}

std::vector<Entry> Pusher::entries()
{
    std::vector<Entry> entries;
    for (std::size_t r = 0; r < variables_; ++r)
    {
        merge(rows_[r]);
        for (const std::uint32_t column : rows_[r])
        {
            entries.push_back({.row = to_index(r), .column = to_index(std::size_t{column})});
        }
    }
    std::ranges::sort(entries, before);
    return entries;
}

}
