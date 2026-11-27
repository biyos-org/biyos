#include "expr/evaluation/threads/sweeps.hpp"

#include "core/env/pool.hpp"
#include "core/numbers.hpp"
#include "expr/evaluation/derivatives/derivative.hpp"
#include "expr/evaluation/derivatives/reverse.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/evaluation/threads/levels.hpp"
#include "expr/evaluation/threads/team.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
#include <cstddef>
#include <span>

namespace biyos
{

namespace
{

enum class Sweep
{
    Values,
    Partials,
    Weighted,
};

struct Job
{
    const Tape &tape;
    std::span<const Real> point;
    Team &team;
    std::span<Real> values;
    std::span<Real> gradients;
    std::span<const Real> weights;
};

void forward_block(const Job &job,
                   const Block &block,
                   Worker &member,
                   std::span<Real> into) noexcept
{
    const Workspace &lead = job.team.lead().workspace;
    forward(block, job.point, lead.shared, member.workspace);
    store(block, member.workspace, into);
}

void linearize_block(const Job &job,
                     const Block &block,
                     std::span<Real> partials,
                     Worker &member,
                     std::span<Real> into) noexcept
{
    const Workspace &lead = job.team.lead().workspace;
    linearize(block, job.point, lead.shared, partials, member.workspace);
    store(block, member.workspace, into);
}

void take(Targets &targets, std::size_t index, const Block &block, Adjoints &adjoints) noexcept
{
    const std::size_t lanes = adjoints.lanes;
    const std::span<Real> parts = parts_of(targets, index, lanes);
    std::size_t k = 0;
    for (const std::size_t variable : variables_of(targets, index))
    {
        drain(row(std::span(adjoints.gradient), variable, lanes), row(parts, k++, lanes));
    }
    for (const Offset load : block.loads)
    {
        drain(row(std::span(adjoints.shared), to_size(load), lanes), row(parts, k++, lanes));
    }
}

void give(const Job &job, std::size_t index) noexcept
{
    Targets &targets = job.team.targets;
    const std::size_t lanes = job.team.lanes;
    const std::span<Real> parts = parts_of(targets, index, lanes);
    const std::span<Real> pinned(job.team.pinned);
    std::size_t k = 0;
    for (const std::size_t variable : variables_of(targets, index))
    {
        drain(row(parts, k++, lanes), row(job.gradients, variable, lanes));
    }
    for (const Offset load : job.tape.blocks()[index].loads)
    {
        drain(row(parts, k++, lanes), row(pinned, to_size(load), lanes));
    }
}

struct Prepass
{
    const Job &job;
    std::span<const std::size_t> blocks;
    Sweep sweep;

    void operator()(std::size_t chunk, std::size_t worker) const noexcept
    {
        const Block &block = job.tape.blocks()[blocks[chunk]];
        Worker &member = job.team.members[worker];
        Workspace &lead = job.team.lead().workspace;
        switch (sweep)
        {
        case Sweep::Values:
            forward_block(job, block, member, lead.shared);
            return;
        case Sweep::Partials:
        case Sweep::Weighted:
            linearize_block(job, block, lead.partials, member, lead.shared);
            return;
        }
    }
};

struct Partition
{
    const Job &job;
    Sweep sweep;

    void operator()(std::size_t chunk, std::size_t worker) const noexcept
    {
        const std::size_t index = to_size(job.tape.prepass()) + chunk;
        const Block &block = job.tape.blocks()[index];
        Worker &member = job.team.members[worker];
        switch (sweep)
        {
        case Sweep::Values:
            forward_block(job, block, member, job.values);
            return;
        case Sweep::Partials:
            linearize_block(job, block, member.workspace.partials, member, job.values);
            differentiate(job.tape,
                          block,
                          member.workspace,
                          job.team.lead().workspace,
                          member.adjoints,
                          job.gradients);
            return;
        case Sweep::Weighted:
            linearize_block(job, block, member.workspace.partials, member, job.values);
            seed_weights(block, job.weights, member.adjoints);
            reverse(block, member.workspace, member.adjoints, member.adjoints.gradient);
            take(job.team.targets, index, block, member.adjoints);
            return;
        }
    }
};

struct Climb
{
    const Job &job;
    std::span<const std::size_t> blocks;

    void operator()(std::size_t chunk, std::size_t worker) const noexcept
    {
        const std::size_t index = blocks[chunk];
        const Block &block = job.tape.blocks()[index];
        Adjoints &adjoints = job.team.members[worker].adjoints;
        pull(block, job.team.pinned, adjoints);
        reverse(block, job.team.lead().workspace, adjoints, adjoints.gradient);
        take(job.team.targets, index, block, adjoints);
    }
};

void prepass(const Job &job, Pool &pool, Sweep sweep)
{
    const Levels &levels = job.team.levels;
    for (std::size_t k = 0; k + 1 < levels.starts.size(); ++k)
    {
        Prepass task{.job = job, .blocks = level(levels, k), .sweep = sweep};
        pool.run(job.team.size(), task.blocks.size(), task);
    }
}

void partitions(const Job &job, Pool &pool, Sweep sweep)
{
    Partition task{.job = job, .sweep = sweep};
    const std::size_t first = to_size(job.tape.prepass());
    pool.run(job.team.size(), job.tape.blocks().size() - first, task);
}

void give_partitions(const Job &job) noexcept
{
    for (std::size_t index = to_size(job.tape.prepass()); index < job.tape.blocks().size(); ++index)
    {
        give(job, index);
    }
}

void climb(const Job &job, Pool &pool)
{
    const Levels &levels = job.team.levels;
    for (std::size_t k = levels.starts.size() - 1; k > 0; --k)
    {
        Climb task{.job = job, .blocks = level(levels, k - 1)};
        pool.run(job.team.size(), task.blocks.size(), task);
        for (const std::size_t index : task.blocks)
        {
            give(job, index);
        }
    }
}

}

void forward(const Tape &tape,
             std::span<const Real> point,
             Team &team,
             Pool &pool,
             std::span<Real> values)
{
    check_sweep(tape, point, team.lead().workspace, values);
    const Job job{.tape = tape,
                  .point = point,
                  .team = team,
                  .values = values,
                  .gradients = {},
                  .weights = {}};
    prepass(job, pool, Sweep::Values);
    partitions(job, pool, Sweep::Values);
}

void jacobian(const Tape &tape,
              std::span<const Real> point,
              Team &team,
              Pool &pool,
              std::span<Real> values,
              std::span<Real> gradients)
{
    Worker &lead = team.lead();
    check_jacobian(tape, point, lead.workspace, lead.adjoints, values, gradients);
    const Job job{.tape = tape,
                  .point = point,
                  .team = team,
                  .values = values,
                  .gradients = gradients,
                  .weights = {}};
    prepass(job, pool, Sweep::Partials);
    partitions(job, pool, Sweep::Partials);
}

void weighted_gradient(const Tape &tape,
                       std::span<const Real> point,
                       std::span<const Real> weights,
                       Team &team,
                       Pool &pool,
                       std::span<Real> values,
                       std::span<Real> gradient)
{
    Worker &lead = team.lead();
    check_weighted(tape, point, weights, lead.workspace, lead.adjoints, values, gradient);
    std::ranges::fill(gradient, 0.0);
    const Job job{.tape = tape,
                  .point = point,
                  .team = team,
                  .values = values,
                  .gradients = gradient,
                  .weights = weights};
    prepass(job, pool, Sweep::Weighted);
    partitions(job, pool, Sweep::Weighted);
    give_partitions(job);
    climb(job, pool);
}

}
