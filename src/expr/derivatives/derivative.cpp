#include "expr/derivatives/derivative.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "expr/derivatives/reverse.hpp"
#include "expr/derivatives/value.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

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
    const std::size_t lanes = lanes_of(tape, workspace.lanes);
    if (workspace.partials.size() < at(tape.partials()) * lanes)
    {
        refuse("workspace", "sized for another tape");
    }
    if (adjoints.lanes != lanes)
    {
        const std::string mine = "lane count " + text_of(adjoints.lanes);
        refuse("adjoints", mine + ", not the workspace's " + text_of(lanes));
    }
    const bool shared = adjoints.shared.size() >= at(tape.pinned()) * lanes;
    const bool gradient = adjoints.gradient.size() >= at(tape.variables()) * lanes;
    const bool slots = adjoints.slots.size() >= tape.width() * lanes;
    if (!shared || !gradient || !slots || adjoints.taken.size() < lanes)
    {
        refuse("adjoints", "sized for another tape");
    }
}

void check_size(std::string_view object,
                std::span<const double> given,
                Offset wanted,
                std::string_view name,
                std::size_t lanes)
{
    if (given.size() != sized(at(wanted), lanes))
    {
        const std::string size = "size " + text_of(given.size()) + ", not " + std::string(name);
        refuse(std::string(object), size + " " + text_of(wanted) + in_lanes(lanes));
    }
}

void prepare(const Tape &tape, std::span<const double> point, Workspace &workspace) noexcept
{
    for (const Block &block : tape.blocks().first(at(tape.prepass())))
    {
        linearize(block, point, workspace);
        store(block, workspace, workspace.shared);
    }
}

void rise(const Block &block,
          Offset position,
          const Workspace &workspace,
          Adjoints &adjoints) noexcept
{
    const std::size_t output = at(position - block.begin);
    const std::size_t lanes = adjoints.lanes;
    const std::span<double> from = row(std::span(adjoints.shared), at(position), lanes);
    drain(from, row(std::span(adjoints.slots), block.outputs[output], lanes));
    retrace(block, output, workspace, adjoints, adjoints.gradient);
}

void descend(std::span<const Block> prepass,
             Range run,
             const Workspace &workspace,
             Adjoints &adjoints) noexcept
{
    auto block = std::ranges::upper_bound(prepass, run.end - 1, {}, &Block::begin) - 1;
    for (Offset position = run.end - 1; position >= run.begin; --position)
    {
        while (position < block->begin)
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

void gather(const Pattern &pattern, Offset root, Adjoints &adjoints, std::span<double> gradients)
{
    const std::size_t lanes = adjoints.lanes;
    for (Offset k = pattern.starts[at(root)]; k < pattern.starts[at(root) + 1]; ++k)
    {
        const auto variable = static_cast<std::size_t>(pattern.variables[at(k)]);
        const std::span<double> from = row(std::span(adjoints.gradient), variable, lanes);
        const std::span<double> into = row(gradients, at(k), lanes);
        std::ranges::copy(from, into.begin());
        std::ranges::fill(from, 0.0);
    }
}

void differentiate(const Tape &tape,
                   const Block &block,
                   const Workspace &workspace,
                   Adjoints &adjoints,
                   std::span<double> gradients) noexcept
{
    const std::size_t lanes = adjoints.lanes;
    for (std::size_t k = 0; k < block.outputs.size(); ++k)
    {
        const Offset root = block.begin + count_of(k);
        std::ranges::fill(row(std::span(adjoints.slots), block.outputs[k], lanes), 1.0);
        retrace(block, k, workspace, adjoints, adjoints.gradient);
        climb(tape, root, workspace, adjoints);
        gather(tape.pattern(), root, adjoints, gradients);
    }
}

void seed(const Block &block, std::span<const double> weights, Adjoints &adjoints) noexcept
{
    const std::size_t lanes = adjoints.lanes;
    for (std::size_t k = 0; k < block.outputs.size(); ++k)
    {
        const std::span<const double> mine = row(weights, at(block.begin) + k, lanes);
        const std::span<double> into = row(std::span(adjoints.slots), block.outputs[k], lanes);
        for (std::size_t l = 0; l < lanes; ++l)
        {
            into[l] += mine[l];
        }
    }
}

void pull(const Block &block, Adjoints &adjoints) noexcept
{
    const std::size_t lanes = adjoints.lanes;
    for (std::size_t k = 0; k < block.outputs.size(); ++k)
    {
        const std::span<double> from = row(std::span(adjoints.shared), at(block.begin) + k, lanes);
        drain(from, row(std::span(adjoints.slots), block.outputs[k], lanes));
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
    check_size("gradients", gradients, entries, "the pattern size", workspace.lanes);
    prepare(tape, point, workspace);
    for (const Block &block : tape.blocks().subspan(at(tape.prepass())))
    {
        linearize(block, point, workspace);
        store(block, workspace, values);
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
    check_size("weights", weights, tape.roots(), "the root count", workspace.lanes);
    check_size("gradient", gradient, count_of(point.size()), "the point size", 1);
    std::ranges::fill(gradient, 0.0);
    prepare(tape, point, workspace);
    const std::span<const Block> blocks = tape.blocks();
    const std::size_t prepass = at(tape.prepass());
    for (const Block &block : blocks.subspan(prepass))
    {
        linearize(block, point, workspace);
        store(block, workspace, values);
        seed(block, weights, adjoints);
        reverse(block, workspace, adjoints, gradient);
    }
    for (const Block &block : std::views::reverse(blocks.first(prepass)))
    {
        pull(block, adjoints);
        reverse(block, workspace, adjoints, gradient);
    }
}

}
