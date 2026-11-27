#include "expr/evaluation/derivatives/derivative.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/text/text.hpp"
#include "expr/evaluation/derivatives/reverse.hpp"
#include "expr/evaluation/derivatives/value.hpp"
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

[[noreturn]] void refuse(std::string object, std::string reason)
{
    throw Error(ErrorKind::InvalidArgument, std::move(object), std::move(reason));
}

void check_reverse(const Tape &tape, const Workspace &workspace, const Adjoints &adjoints)
{
    const std::size_t lanes = lanes_of(tape, workspace.lanes);
    if (workspace.partials.size() < to_size(tape.partials()) * lanes)
    {
        refuse("workspace", "sized for another tape");
    }
    if (adjoints.lanes != lanes)
    {
        const std::string mine = "lane count " + text_of(adjoints.lanes);
        refuse("adjoints", mine + ", not the workspace's " + text_of(lanes));
    }
    const bool shared = adjoints.shared.size() >= to_size(tape.pinned()) * lanes;
    const bool gradient = adjoints.gradient.size() >= to_size(tape.variables()) * lanes;
    const bool slots = adjoints.slots.size() >= tape.width() * lanes;
    if (!shared || !gradient || !slots || adjoints.taken.size() < lanes)
    {
        refuse("adjoints", "sized for another tape");
    }
}

void check_size(std::string_view object,
                std::span<const Real> given,
                Offset wanted,
                std::string_view name,
                std::size_t lanes)
{
    if (given.size() != sized(to_size(wanted), lanes))
    {
        const std::string size = "size " + text_of(given.size()) + ", not " + std::string(name);
        refuse(std::string(object), size + " " + text_of(wanted) + in_lanes(lanes));
    }
}

void prepare(const Tape &tape,
             std::span<const Real> point,
             const Tolerances &tolerances,
             Workspace &workspace) noexcept
{
    for (const Block &block : tape.blocks().first(to_size(tape.prepass())))
    {
        linearize(block, point, tolerances, workspace);
        store(block, workspace, workspace.shared);
    }
}

void rise(const Block &block,
          Offset position,
          const Workspace &workspace,
          Adjoints &adjoints) noexcept
{
    const std::size_t output = to_size(position - block.begin);
    const std::size_t lanes = adjoints.lanes;
    const std::span<Real> from = row(std::span(adjoints.shared), to_size(position), lanes);
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
    const std::span<const Block> prepass = tape.blocks().first(to_size(tape.prepass()));
    for (const Range run : std::views::reverse(tape.route(root)))
    {
        descend(prepass, run, workspace, adjoints);
    }
}

void gather(const Pattern &pattern, Offset root, Adjoints &adjoints, std::span<Real> gradients)
{
    const std::size_t lanes = adjoints.lanes;
    for (Offset k = pattern.starts[to_size(root)]; k < pattern.starts[to_size(root) + 1]; ++k)
    {
        const auto variable = to_size(pattern.variables[to_size(k)]);
        const std::span<Real> from = row(std::span(adjoints.gradient), variable, lanes);
        const std::span<Real> into = row(gradients, to_size(k), lanes);
        std::ranges::copy(from, into.begin());
        std::ranges::fill(from, 0.0);
    }
}

}

void check_jacobian(const Tape &tape,
                    std::span<const Real> point,
                    const Workspace &workspace,
                    const Adjoints &adjoints,
                    std::span<const Real> values,
                    std::span<const Real> gradients)
{
    check_sweep(tape, point, workspace, values);
    check_reverse(tape, workspace, adjoints);
    const Offset entries = to_offset(tape.pattern().variables.size());
    check_size("gradients", gradients, entries, "the pattern size", workspace.lanes);
}

void check_weighted(const Tape &tape,
                    std::span<const Real> point,
                    std::span<const Real> weights,
                    const Workspace &workspace,
                    const Adjoints &adjoints,
                    std::span<const Real> values,
                    std::span<const Real> gradient)
{
    check_sweep(tape, point, workspace, values);
    check_reverse(tape, workspace, adjoints);
    check_size("weights", weights, tape.roots(), "the root count", workspace.lanes);
    check_size("gradient", gradient, to_offset(point.size()), "the point size", 1);
}

void differentiate(const Tape &tape,
                   const Block &block,
                   const Workspace &workspace,
                   const Workspace &prepass,
                   Adjoints &adjoints,
                   std::span<Real> gradients) noexcept
{
    const std::size_t lanes = adjoints.lanes;
    for (std::size_t k = 0; k < block.outputs.size(); ++k)
    {
        const Offset root = block.begin + to_offset(k);
        std::ranges::fill(row(std::span(adjoints.slots), block.outputs[k], lanes), 1.0);
        retrace(block, k, workspace, adjoints, adjoints.gradient);
        climb(tape, root, prepass, adjoints);
        gather(tape.pattern(), root, adjoints, gradients);
    }
}

void seed_weights(const Block &block, std::span<const Real> weights, Adjoints &adjoints) noexcept
{
    const std::size_t lanes = adjoints.lanes;
    for (std::size_t k = 0; k < block.outputs.size(); ++k)
    {
        const std::span<const Real> mine = row(weights, to_size(block.begin) + k, lanes);
        const std::span<Real> into = row(std::span(adjoints.slots), block.outputs[k], lanes);
        for (std::size_t l = 0; l < lanes; ++l)
        {
            into[l] += mine[l];
        }
    }
}

void pull(const Block &block, std::span<Real> shared, Adjoints &adjoints) noexcept
{
    const std::size_t lanes = adjoints.lanes;
    for (std::size_t k = 0; k < block.outputs.size(); ++k)
    {
        const std::span<Real> from = row(shared, to_size(block.begin) + k, lanes);
        drain(from, row(std::span(adjoints.slots), block.outputs[k], lanes));
    }
}

void jacobian(const Tape &tape,
              std::span<const Real> point,
              const Tolerances &tolerances,
              Workspace &workspace,
              Adjoints &adjoints,
              std::span<Real> values,
              std::span<Real> gradients)
{
    check_jacobian(tape, point, workspace, adjoints, values, gradients);
    prepare(tape, point, tolerances, workspace);
    for (const Block &block : tape.blocks().subspan(to_size(tape.prepass())))
    {
        linearize(block, point, tolerances, workspace);
        store(block, workspace, values);
        differentiate(tape, block, workspace, workspace, adjoints, gradients);
    }
}

void weighted_gradient(const Tape &tape,
                       std::span<const Real> point,
                       std::span<const Real> weights,
                       const Tolerances &tolerances,
                       Workspace &workspace,
                       Adjoints &adjoints,
                       std::span<Real> values,
                       std::span<Real> gradient)
{
    check_weighted(tape, point, weights, workspace, adjoints, values, gradient);
    std::ranges::fill(gradient, 0.0);
    prepare(tape, point, tolerances, workspace);
    const std::span<const Block> blocks = tape.blocks();
    const std::size_t prepass = to_size(tape.prepass());
    for (const Block &block : blocks.subspan(prepass))
    {
        linearize(block, point, tolerances, workspace);
        store(block, workspace, values);
        seed_weights(block, weights, adjoints);
        reverse(block, workspace, adjoints, gradient);
    }
    for (const Block &block : std::views::reverse(blocks.first(prepass)))
    {
        pull(block, adjoints.shared, adjoints);
        reverse(block, workspace, adjoints, gradient);
    }
}

}
