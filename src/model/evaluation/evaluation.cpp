#include "model/evaluation/evaluation.hpp"

#include "core/env.hpp"
#include "core/index.hpp"
#include "core/timing/timer.hpp"
#include "expr/hessian/elements.hpp"
#include "expr/hessian/hessian.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/tape.hpp"
#include "model/evaluation/maps.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

Compiled compiled_of(Env &env, const Model &model)
{
    const EntryTimer timer(env, "model", "evaluation");
    std::vector<Index> rooted;
    std::vector<Offset> roots;
    const std::span<const Offset> all = model.roots(0, model.n_functions());
    for (std::size_t k = 0; k < all.size(); ++k)
    {
        if (all[k] != kNoRoot)
        {
            rooted.push_back(static_cast<Index>(k));
            roots.push_back(all[k]);
        }
    }
    Tape tape(model.graph(), roots);
    Slots slots = slots_of(model, rooted, tape.pattern());
    Declared declared = declared_of(model);
    Hessian hessian(model.graph(), roots, declared.entries);
    return Compiled{
        .rooted = std::move(rooted),
        .roots = std::move(roots),
        .tape = std::move(tape),
        .slots = std::move(slots),
        .declared = std::move(declared),
        .hessian = std::move(hessian),
    };
}

}

Evaluation::Evaluation(Env &env, const Model &model) : compiled_(compiled_of(env, model)) {}

const Pattern &Evaluation::jacobian_pattern() const noexcept
{
    return compiled_.slots.pattern;
}

const Lower &Evaluation::hessian_pattern() const noexcept
{
    return compiled_.hessian.pattern();
}

}
