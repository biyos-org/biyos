#include "expr/evaluation/threads/team.hpp"

#include "core/memory/resize.hpp"
#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "expr/evaluation/derivatives/reverse.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/evaluation/threads/levels.hpp"
#include "expr/graph/node.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

void add_variables(const Block &block, std::vector<std::size_t> &variables)
{
    const auto first = static_cast<std::ptrdiff_t>(variables.size());
    for (const Instruction &instruction : block.instructions)
    {
        if (instruction.op == Operator::Variable)
        {
            variables.push_back(place(instruction.payload));
        }
    }
    std::sort(variables.begin() + first, variables.end());
    variables.erase(std::unique(variables.begin() + first, variables.end()), variables.end());
}

Targets targets_of(const Tape &tape, std::size_t lanes)
{
    Targets targets{.variables = {}, .starts = {0}, .rows = {0}, .parts = {}};
    for (const Block &block : tape.blocks())
    {
        add_variables(block, targets.variables);
        const Offset added = to_offset(targets.variables.size()) - targets.starts.back();
        targets.starts.push_back(to_offset(targets.variables.size()));
        targets.rows.push_back(targets.rows.back() + added + to_offset(block.loads.size()));
    }
    targets.parts.assign(to_size(targets.rows.back()) * lanes, 0.0);
    return targets;
}

}

Worker::Worker(const Tape &tape, std::size_t lanes) : workspace(tape, lanes), adjoints(tape, lanes)
{
}

Team::Team(const Tape &tape, std::size_t width)
    : lanes(width), members{Worker(tape, width)}, levels(levels_of(tape)),
      targets(targets_of(tape, width)), pinned(to_size(tape.pinned()) * width, 0.0)
{
}

void Team::fit(const Tape &tape, std::size_t threads)
{
    resize(members, threads, [&] { return Worker(tape, lanes); });
}

std::size_t Team::size() const noexcept
{
    return members.size();
}

Worker &Team::lead() noexcept
{
    return members.front();
}

std::span<const std::size_t> variables_of(const Targets &targets, std::size_t block) noexcept
{
    return slice(std::span(targets.variables), targets.starts, block);
}

std::span<Real> parts_of(Targets &targets, std::size_t block, std::size_t lanes) noexcept
{
    const std::size_t begin = to_size(targets.rows[block]) * lanes;
    const std::size_t end = to_size(targets.rows[block + 1]) * lanes;
    return std::span(targets.parts).subspan(begin, end - begin);
}

}
