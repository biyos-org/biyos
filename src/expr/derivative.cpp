#include "expr/derivative.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text.hpp"
#include "expr/reach.hpp"
#include "expr/reverse.hpp"
#include "expr/tape.hpp"
#include "expr/value.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace biyos
{

namespace
{

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

Offset count_of(std::size_t value) noexcept
{
    return static_cast<Offset>(value);
}

[[noreturn]] void refuse(std::string object, std::string reason)
{
    throw Error(ErrorKind::InvalidArgument, std::move(object), std::move(reason));
}

void check_reverse(const Tape &tape, const Workspace &workspace, const Adjoints &adjoints)
{
    if (count_of(workspace.partials.size()) < tape.partials())
    {
        refuse("workspace", "sized for another tape");
    }
    const bool shared = count_of(adjoints.shared.size()) >= tape.pinned();
    const bool gradient = count_of(adjoints.gradient.size()) >= tape.variables();
    if (!shared || !gradient || adjoints.slots.size() < tape.width())
    {
        refuse("adjoints", "sized for another tape");
    }
}

void check_size(std::string_view object,
                std::span<const double> given,
                Offset wanted,
                std::string_view name)
{
    const Offset size = count_of(given.size());
    if (size != wanted)
    {
        const std::string reason = "size " + text_of(size) + ", not " + std::string(name);
        refuse(std::string(object), reason + " " + text_of(wanted));
    }
}

void prepare(const Tape &tape, std::span<const double> point, Workspace &workspace) noexcept
{
    for (const Block &block : tape.blocks().first(at(tape.prepass())))
    {
        linearize(block, point, workspace);
        store(block, workspace.slots, workspace.shared);
    }
}

void rise(const Block &block,
          Offset position,
          const Workspace &workspace,
          Adjoints &adjoints) noexcept
{
    const std::size_t output = at(position - block.first);
    const double adjoint = std::exchange(adjoints.shared[at(position)], 0.0);
    adjoints.slots[block.outputs[output]] += adjoint;
    retrace(block, output, workspace, adjoints, adjoints.gradient);
}

void descend(std::span<const Block> prepass,
             Range run,
             const Workspace &workspace,
             Adjoints &adjoints) noexcept
{
    auto block = std::ranges::upper_bound(prepass, run.end - 1, {}, &Block::first) - 1;
    for (Offset position = run.end - 1; position >= run.begin; --position)
    {
        while (position < block->first)
        {
            --block;
        }
        rise(*block, position, workspace, adjoints);
    }
}

void climb(const Tape &tape, Offset root, const Workspace &workspace, Adjoints &adjoints) noexcept
{
    const std::span<const Block> prepass = tape.blocks().first(at(tape.prepass()));
    for (const Range run : std::views::reverse(tape.route(root)))
    {
        descend(prepass, run, workspace, adjoints);
    }
}

void gather(const Pattern &pattern,
            Offset root,
            std::span<double> gradient,
            std::span<double> gradients) noexcept
{
    for (Offset k = pattern.starts[at(root)]; k < pattern.starts[at(root) + 1]; ++k)
    {
        const auto variable = static_cast<std::size_t>(pattern.variables[at(k)]);
        gradients[at(k)] = std::exchange(gradient[variable], 0.0);
    }
}

void differentiate(const Tape &tape,
                   const Block &block,
                   const Workspace &workspace,
                   Adjoints &adjoints,
                   std::span<double> gradients) noexcept
{
    for (std::size_t k = 0; k < block.outputs.size(); ++k)
    {
        const Offset root = block.first + count_of(k);
        adjoints.slots[block.outputs[k]] = 1.0;
        retrace(block, k, workspace, adjoints, adjoints.gradient);
        climb(tape, root, workspace, adjoints);
        gather(tape.pattern(), root, adjoints.gradient, gradients);
    }
}

void seed(const Block &block, std::span<const double> weights, std::span<double> slots) noexcept
{
    const std::span<const double> mine = weights.subspan(at(block.first), block.outputs.size());
    for (std::size_t k = 0; k < mine.size(); ++k)
    {
        slots[block.outputs[k]] += mine[k];
    }
}

void pull(const Block &block, Adjoints &adjoints) noexcept
{
    const std::span<double> shared = std::span(adjoints.shared);
    const std::span<double> mine = shared.subspan(at(block.first), block.outputs.size());
    for (std::size_t k = 0; k < mine.size(); ++k)
    {
        adjoints.slots[block.outputs[k]] += std::exchange(mine[k], 0.0);
    }
}

}

void jacobian(const Tape &tape,
              std::span<const double> point,
              Workspace &workspace,
              Adjoints &adjoints,
              std::span<double> values,
              std::span<double> gradients)
{
    check_sweep(tape, point, workspace, values);
    check_reverse(tape, workspace, adjoints);
    const Offset entries = count_of(tape.pattern().variables.size());
    check_size("gradients", gradients, entries, "the pattern size");
    prepare(tape, point, workspace);
    for (const Block &block : tape.blocks().subspan(at(tape.prepass())))
    {
        linearize(block, point, workspace);
        store(block, workspace.slots, values);
        differentiate(tape, block, workspace, adjoints, gradients);
    }
}

void weighted_gradient(const Tape &tape,
                       std::span<const double> point,
                       std::span<const double> weights,
                       Workspace &workspace,
                       Adjoints &adjoints,
                       std::span<double> values,
                       std::span<double> gradient)
{
    check_sweep(tape, point, workspace, values);
    check_reverse(tape, workspace, adjoints);
    check_size("weights", weights, tape.roots(), "the root count");
    check_size("gradient", gradient, count_of(point.size()), "the point size");
    std::ranges::fill(gradient, 0.0);
    prepare(tape, point, workspace);
    const std::span<const Block> blocks = tape.blocks();
    const std::size_t prepass = at(tape.prepass());
    for (const Block &block : blocks.subspan(prepass))
    {
        linearize(block, point, workspace);
        store(block, workspace.slots, values);
        seed(block, weights, adjoints.slots);
        reverse(block, workspace, adjoints, gradient);
    }
    for (const Block &block : std::views::reverse(blocks.first(prepass)))
    {
        pull(block, adjoints);
        reverse(block, workspace, adjoints, gradient);
    }
}

}
