#include "expr/evaluation/hessian/pushing.hpp"

#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/numeric/zero.hpp"
#include "expr/evaluation/derivatives/partials.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/evaluation/hessian/pattern.hpp"
#include "expr/evaluation/hessian/tangents.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/layout.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

constexpr Offset kNone = -1;
constexpr std::size_t kSingle = 1;

struct Widest
{
    std::size_t capacity;
    std::size_t ids;
    std::size_t operands;
    std::size_t bends;
};

struct Walk
{
    std::span<const Real> point;
    const Tolerances &tolerances;
    Workspace &workspace;
    const Elimination &elimination;
    PushSpace &space;
    std::size_t node;
};

struct Push
{
    const Elimination &elimination;
    PushSpace &space;
};

std::uint32_t bends_in(const Node &node) noexcept
{
    const std::uint32_t n = node.end - node.begin;
    switch (node.bending)
    {
    case Bending::None:
        return 0;
    case Bending::Full:
        return n * (n + 1) / 2;
    case Bending::Quotient:
        return 2;
    case Bending::Cross:
        return n * (n - 1) / 2;
    }
    return 0;
}

std::vector<std::uint32_t> bend_starts(std::span<const Node> nodes)
{
    std::vector<std::uint32_t> starts{0};
    for (const Node &node : nodes)
    {
        starts.push_back(starts.back() + bends_in(node));
    }
    return starts;
}

std::vector<Offset> row_starts(std::span<const Offset> links)
{
    std::vector<Offset> starts{0};
    for (const Offset count : links)
    {
        starts.push_back(starts.back() + count);
    }
    return starts;
}

void index_rows(std::span<const Entry> entries, Elimination &elimination)
{
    std::vector<Offset> &starts = elimination.starts;
    starts.assign(std::size_t{elimination.variables} + 1, 0);
    for (const Entry entry : entries)
    {
        ++starts[to_size(entry.row) + 1];
    }
    std::partial_sum(starts.begin(), starts.end(), starts.begin());
    std::vector<Offset> next(starts.begin(), starts.end() - 1);
    elimination.columns.assign(entries.size(), 0);
    elimination.parts.assign(entries.size(), 0);
    for (std::size_t k = 0; k < entries.size(); ++k)
    {
        const auto at = to_size(next[to_size(entries[k].row)]++);
        elimination.columns[at] = narrow(to_size(entries[k].column));
        elimination.parts[at] = to_offset(k);
    }
}

Widest widest_of(std::span<const Elimination> eliminations) noexcept
{
    Widest widest{.capacity = 0, .ids = 0, .operands = 0, .bends = 0};
    for (const Elimination &elimination : eliminations)
    {
        widest.capacity = (std::max)(widest.capacity, to_size(elimination.rows.back()));
        widest.ids = (std::max)(widest.ids, elimination.rows.size() - 1);
        widest.operands = (std::max)(widest.operands, elimination.operands.size());
        widest.bends = (std::max)(widest.bends, std::size_t{elimination.bends.back()});
    }
    return widest;
}

Real times(Real a, Real b) noexcept
{
    return is_zero(a) || is_zero(b) ? 0.0 : a * b;
}

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

void link(const Push &push, std::uint32_t a, std::uint32_t b, Real value, bool across) noexcept
{
    if (is_zero(value))
    {
        return;
    }
    const std::uint32_t row = (std::max)(a, b);
    const auto at = to_size(push.elimination.rows[row] + push.space.sizes[row]++);
    push.space.columns[at] = (std::min)(a, b);
    push.space.values[at] = across && a == b ? 2.0 * value : value;
}

Offset merge(const Push &push, std::size_t row) noexcept
{
    PushSpace &space = push.space;
    const Offset begin = push.elimination.rows[row];
    const Offset end = begin + std::exchange(space.sizes[row], 0);
    Offset merged = begin;
    for (Offset k = begin; k < end; ++k)
    {
        const std::uint32_t column = space.columns[to_size(k)];
        Offset &mark = space.marks[column];
        if (mark == kNone)
        {
            mark = merged;
            space.columns[to_size(merged)] = column;
            space.values[to_size(merged)] = space.values[to_size(k)];
            ++merged;
        }
        else
        {
            space.values[to_size(mark)] += space.values[to_size(k)];
        }
    }
    for (Offset k = begin; k < merged; ++k)
    {
        space.marks[space.columns[to_size(k)]] = kNone;
    }
    return merged;
}

void spread(const Push &push,
            std::span<const std::uint32_t> operands,
            std::span<const Real> slopes,
            std::uint32_t neighbour,
            Real weight) noexcept
{
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        link(push, operands[k], neighbour, times(weight, slopes[k]), true);
    }
}

void square(const Push &push,
            std::span<const std::uint32_t> operands,
            std::span<const Real> slopes,
            Real weight) noexcept
{
    for (std::size_t k = 0; k < operands.size(); ++k)
    {
        const Real along = times(weight, slopes[k]);
        for (std::size_t m = k; m < operands.size() && !is_zero(along); ++m)
        {
            link(push, operands[k], operands[m], times(along, slopes[m]), k != m);
        }
    }
}

void create(const Push &push,
            const Node &node,
            std::span<const std::uint32_t> operands,
            std::span<const Real> bends,
            Real adjoint) noexcept
{
    std::size_t at = 0;
    switch (node.bending)
    {
    case Bending::None:
        return;
    case Bending::Full:
        for (std::size_t k = 0; k < operands.size(); ++k)
        {
            for (std::size_t m = k; m < operands.size(); ++m)
            {
                link(push, operands[k], operands[m], scaled(adjoint, bends[at++]), k != m);
            }
        }
        return;
    case Bending::Quotient:
        link(push, operands.front(), operands.back(), scaled(adjoint, bends[0]), true);
        link(push, operands.back(), operands.back(), scaled(adjoint, bends[1]), false);
        return;
    case Bending::Cross:
        for (std::size_t k = 0; k < operands.size(); ++k)
        {
            for (std::size_t m = k + 1; m < operands.size(); ++m)
            {
                link(push, operands[k], operands[m], scaled(adjoint, bends[at++]), true);
            }
        }
        return;
    }
}

void eliminate(const Push &push, std::size_t n) noexcept
{
    const Elimination &elimination = push.elimination;
    PushSpace &space = push.space;
    const Node &node = elimination.nodes[n];
    const std::size_t id = elimination.variables + n;
    const std::size_t count = node.end - node.begin;
    const std::span<const std::uint32_t> operands =
        std::span(elimination.operands).subspan(node.begin, count);
    const std::span<const Real> slopes = std::span(space.slopes).subspan(node.begin, count);
    const Offset end = merge(push, id);
    for (auto k = to_size(elimination.rows[id]); k < to_size(end); ++k)
    {
        const std::uint32_t neighbour = space.columns[k];
        const Real weight = space.values[k];
        if (neighbour == id)
        {
            square(push, operands, slopes, weight);
        }
        else
        {
            spread(push, operands, slopes, neighbour, weight);
        }
    }
    const Real adjoint = std::exchange(space.adjoints[id], 0.0);
    if (is_zero(adjoint))
    {
        return;
    }
    for (std::size_t k = 0; k < count; ++k)
    {
        space.adjoints[operands[k]] += times(adjoint, slopes[k]);
    }
    const std::uint32_t first = elimination.bends[n];
    const std::span<const Real> bends =
        std::span(space.bends).subspan(first, elimination.bends[n + 1] - first);
    create(push, node, operands, bends, adjoint);
}

void scatter(const Push &push, std::uint32_t row, std::span<Real> parts) noexcept
{
    const Elimination &elimination = push.elimination;
    PushSpace &space = push.space;
    const std::span<const std::uint32_t> columns =
        slice(std::span(elimination.columns), elimination.starts, row);
    const std::span<const Offset> places =
        slice(std::span(elimination.parts), elimination.starts, row);
    for (std::size_t k = 0; k < columns.size(); ++k)
    {
        space.marks[columns[k]] = places[k];
    }
    const auto begin = to_size(elimination.rows[row]);
    const auto end = begin + to_size(std::exchange(space.sizes[row], 0));
    for (std::size_t k = begin; k < end; ++k)
    {
        parts[to_size(space.marks[space.columns[k]])] += space.values[k];
    }
    for (const std::uint32_t column : columns)
    {
        space.marks[column] = kNone;
    }
    space.adjoints[row] = 0.0;
}

}

PushSpace space_of(const Tape &tape, std::span<const Elimination> eliminations)
{
    const Widest widest = widest_of(eliminations);
    return {
        .columns = std::vector<std::uint32_t>(widest.capacity, 0),
        .values = std::vector<Real>(widest.capacity, 0.0),
        .sizes = std::vector<Offset>(widest.ids, 0),
        .marks = std::vector<Offset>(widest.ids, kNone),
        .adjoints = std::vector<Real>(widest.ids, 0.0),
        .slopes = std::vector<Real>(widest.operands, 0.0),
        .bends = std::vector<Real>(widest.bends, 0.0),
        .suffix = std::vector<Real>(widest_product(tape) + 1, 0.0),
    };
}

Elimination elimination_of(Offset element, Shape shape)
{
    std::vector<std::uint32_t> bends = bend_starts(shape.nodes);
    std::vector<Offset> rows = row_starts(shape.links);
    Elimination elimination{
        .element = element,
        .variables = narrow(shape.links.size() - shape.nodes.size()),
        .root = shape.root,
        .nodes = std::move(shape.nodes),
        .operands = std::move(shape.operands),
        .bends = std::move(bends),
        .rows = std::move(rows),
        .starts = {},
        .columns = {},
        .parts = {},
    };
    index_rows(shape.entries, elimination);
    return elimination;
}

void push(const Tape &tape,
          const Elimination &elimination,
          std::span<const Real> point,
          const Tolerances &tolerances,
          Workspace &workspace,
          PushSpace &space,
          std::span<Real> parts) noexcept
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
    std::ranges::fill(parts, 0.0);
    const Push pushing{.elimination = elimination, .space = space};
    space.adjoints[elimination.root] = 1.0;
    for (std::size_t n = elimination.nodes.size(); n > 0; --n)
    {
        eliminate(pushing, n - 1);
    }
    for (std::uint32_t row = 0; row < elimination.variables; ++row)
    {
        scatter(pushing, row, parts);
    }
}

}
