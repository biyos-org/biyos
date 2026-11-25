#include "model/evaluation/evaluation.hpp"

#include "biyos/biyos.h"
#include "core/check.hpp"
#include "core/env.hpp"
#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/options/lookup.hpp"
#include "core/text/text.hpp"
#include "core/timing/timer.hpp"
#include "expr/derivatives/derivative.hpp"
#include "expr/derivatives/reverse.hpp"
#include "expr/derivatives/value.hpp"
#include "expr/forms/terms.hpp"
#include "expr/hessian/elements.hpp"
#include "expr/hessian/hessian.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/tape.hpp"
#include "model/evaluation/maps.hpp"
#include "model/evaluation/user.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::size_t kCalls = 4;

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
            rooted.push_back(to_index(k));
            roots.push_back(all[k]);
        }
    }
    Tape tape(model.graph(), roots);
    Slots slots = slots_of(model, rooted, tape.pattern());
    Declared declared = declared_of(model);
    Hessian hessian(model.graph(), roots, declared.entries);
    hessian.widen(model.n_variables());
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

Evaluation::Evaluation(Env &env, const Model &model, const BIYOS_model *handle)
    : env_(env), model_(model), handle_(handle), compiled_(compiled_of(env, model)),
      workspace_(compiled_.tape), adjoints_(compiled_.tape),
      tape_values_(to_size(compiled_.tape.roots())),
      tape_gradients_(compiled_.tape.pattern().variables.size()), out_(widest(model)),
      root_weights_(compiled_.roots.size()), user_weights_(widest(model))
{
    tree_.reserve(kCalls);
}

const Pattern &Evaluation::jacobian_pattern() const noexcept
{
    return compiled_.slots.pattern;
}

const Lower &Evaluation::hessian_pattern() const noexcept
{
    return compiled_.hessian.pattern();
}

void Evaluation::values(const Lookup &lookup, std::span<const Real> x, std::span<Real> f)
{
    const Timer timer(env_, tree_, "model", "values");
    check_point(x);
    check_size("f", f.size(), to_size(model_.n_functions()));
    require_all(model_, Part::Function);
    term_values(x, f);
    forward(compiled_.tape, x, workspace_, tape_values_);
    for (std::size_t e = 0; e < compiled_.rooted.size(); ++e)
    {
        f[to_size(compiled_.rooted[e])] += tape_values_[e];
    }
    add_user_values(Checker(env_, lookup, "model"), x, f);
}

void Evaluation::jacobian(const Lookup &lookup, std::span<const Real> x, std::span<Real> g)
{
    const Timer timer(env_, tree_, "model", "jacobian");
    check_point(x);
    const Slots &slots = compiled_.slots;
    check_size("g", g.size(), slots.pattern.variables.size());
    require_all(model_, Part::Jacobian);
    term_gradients(x, g);
    biyos::jacobian(compiled_.tape, x, workspace_, adjoints_, tape_values_, tape_gradients_);
    for (std::size_t p = 0; p < slots.tape.size(); ++p)
    {
        g[to_size(slots.tape[p])] += tape_gradients_[p];
    }
    add_user_gradients(Checker(env_, lookup, "model"), x, g);
}

void Evaluation::check_size(std::string_view argument, std::size_t size, std::size_t wanted)
{
    if (size != wanted)
    {
        throw Error(ErrorKind::InvalidArgument,
                    "argument '" + std::string(argument) + "'",
                    "size " + text_of(size) + ", not " + text_of(wanted));
    }
}

void Evaluation::check_point(std::span<const Real> x) const
{
    check_size("x", x.size(), to_size(model_.n_variables()));
}

Request Evaluation::request_of(Index callback,
                               std::span<const Real> x,
                               std::span<const Real> w,
                               std::span<const Real> v) const noexcept
{
    return Request{.model = handle_, .callback = callback, .thread = 0, .x = x, .w = w, .v = v};
}

void Evaluation::term_values(std::span<const Real> x, std::span<Real> f) const
{
    const Index n = model_.n_functions();
    const std::span<const Real> constants = model_.constants(0, n);
    const std::span<const LinearTerm> linear = model_.linear_terms(0, n);
    const std::span<const QuadraticTerm> quadratic = model_.quadratic_terms(0, n);
    const Slots &slots = compiled_.slots;
    for (std::size_t k = 0; k < to_size(n); ++k)
    {
        f[k] = constants[k];
        for (Offset t = slots.linear_starts[k]; t < slots.linear_starts[k + 1]; ++t)
        {
            const LinearTerm &term = linear[to_size(t)];
            f[k] += term.coefficient * x[to_size(term.variable)];
        }
        for (Offset t = slots.quadratic_starts[k]; t < slots.quadratic_starts[k + 1]; ++t)
        {
            const QuadraticTerm &term = quadratic[to_size(t)];
            f[k] += term.coefficient * x[to_size(term.row)] * x[to_size(term.column)];
        }
    }
}

void Evaluation::term_gradients(std::span<const Real> x, std::span<Real> g) const
{
    const Slots &slots = compiled_.slots;
    std::ranges::copy(slots.linear, g.begin());
    const std::span<const QuadraticTerm> quadratic =
        model_.quadratic_terms(0, model_.n_functions());
    for (std::size_t t = 0; t < quadratic.size(); ++t)
    {
        const QuadraticTerm &term = quadratic[t];
        g[to_size(slots.quadratic[2 * t])] += term.coefficient * x[to_size(term.column)];
        g[to_size(slots.quadratic[2 * t + 1])] += term.coefficient * x[to_size(term.row)];
    }
}

void Evaluation::add_user_values(const Checker &checker, std::span<const Real> x, std::span<Real> f)
{
    for (Index c = 0; c < model_.n_callbacks(); ++c)
    {
        const Callback &callback = model_.callback(c);
        const std::span<Real> out = std::span(out_).first(callback.covered.size());
        run(checker, callback, Part::Function, request_of(c, x), out);
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            f[to_size(callback.covered[i])] += out[i];
        }
    }
}

void Evaluation::add_user_gradients(const Checker &checker,
                                    std::span<const Real> x,
                                    std::span<Real> g)
{
    const Slots &slots = compiled_.slots;
    for (Index c = 0; c < model_.n_callbacks(); ++c)
    {
        const Callback &callback = model_.callback(c);
        const std::span<Real> out = std::span(out_).first(callback.jacobian_pairs.size());
        run(checker, callback, Part::Jacobian, request_of(c, x), out);
        const std::size_t begin = to_size(slots.callback_starts[to_size(c)]);
        for (std::size_t p = 0; p < out.size(); ++p)
        {
            g[to_size(slots.callbacks[begin + p])] += out[p];
        }
    }
}

}
