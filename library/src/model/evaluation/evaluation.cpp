#include "model/evaluation/evaluation.hpp"

#include "biyos/biyos.h"
#include "core/check.hpp"
#include "core/error.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/options/lookup.hpp"
#include "core/text/text.hpp"
#include "core/timing/timer.hpp"
#include "expr/forms/terms.hpp"
#include "expr/hessian/elements.hpp"
#include "expr/hessian/hessian.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/tape.hpp"
#include "expr/threads/sweeps.hpp"
#include "expr/threads/team.hpp"
#include "model/evaluation/callbacks.hpp"
#include "model/evaluation/layout.hpp"
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

using evaluation::Entries;
using evaluation::entries_of;
using evaluation::evaluate;
using evaluation::layout_of;
using evaluation::linear_of;
using evaluation::Part;
using evaluation::Program;
using evaluation::read_by;
using evaluation::require;
using evaluation::widest;

namespace
{

constexpr std::size_t kSpans = 4;

Program program_of(const Model &model, Stream stream)
{
    const EntryTimer timer(stream, kModule, "evaluation");
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
    evaluation::Layout layout = layout_of(model, rooted, tape.pattern());
    std::vector<Real> linear = linear_of(model, layout);
    Entries entries = entries_of(model);
    Hessian hessian(model.graph(), roots, entries.items);
    hessian.widen(model.n_variables());
    return Program{
        .rooted = std::move(rooted),
        .roots = std::move(roots),
        .tape = std::move(tape),
        .layout = std::move(layout),
        .linear = std::move(linear),
        .entries = std::move(entries),
        .hessian = std::move(hessian),
    };
}

}

Evaluation::Evaluation(const Model &model, Stream stream, Pool &pool)
    : model_(model), stream_(stream), pool_(pool), program_(program_of(model, stream)),
      team_(program_.tape), values_(to_size(program_.tape.roots())),
      gradients_(program_.tape.pattern().variables.size()), out_(widest(model)),
      weights_(program_.roots.size()), covered_(out_.size()), point_(out_.size()),
      direction_(out_.size())
{
    tree_.reserve(kSpans);
}

const Pattern &Evaluation::jacobian_pattern() const noexcept
{
    return program_.layout.pattern;
}

const Lower &Evaluation::hessian_pattern() const noexcept
{
    return program_.hessian.pattern();
}

void Evaluation::values(const Lookup &lookup, std::span<const Real> x, std::span<Real> f)
{
    const Timer timer(stream_, tree_, kModule, "values");
    check(x);
    check("f", f.size(), to_size(model_.n_functions()));
    require(model_, Part::Function);
    fit(lookup);
    term_function(x, f);
    forward(program_.tape, x, team_, pool_, values_);
    for (std::size_t e = 0; e < program_.rooted.size(); ++e)
    {
        f[to_size(program_.rooted[e])] += values_[e];
    }
    evaluate_function(Checker(stream_, lookup, kModule), {.x = x, .w = {}, .v = {}}, f);
}

void Evaluation::jacobian(const Lookup &lookup, std::span<const Real> x, std::span<Real> g)
{
    const Timer timer(stream_, tree_, kModule, "jacobian");
    check(x);
    const evaluation::Layout &layout = program_.layout;
    check("g", g.size(), layout.pattern.variables.size());
    require(model_, Part::Jacobian);
    fit(lookup);
    term_jacobian(x, g);
    biyos::jacobian(program_.tape, x, team_, pool_, values_, gradients_);
    for (std::size_t p = 0; p < layout.slots.tape.size(); ++p)
    {
        g[to_size(layout.slots.tape[p])] += gradients_[p];
    }
    evaluate_jacobian(Checker(stream_, lookup, kModule), {.x = x, .w = {}, .v = {}}, g);
}

void Evaluation::check(std::string_view argument, std::size_t size, std::size_t wanted)
{
    if (size != wanted)
    {
        throw Error(ErrorKind::InvalidArgument,
                    "argument '" + std::string(argument) + "'",
                    "size " + text_of(size) + ", not " + text_of(wanted));
    }
}

void Evaluation::check(std::span<const Real> x) const
{
    check("x", x.size(), to_size(model_.n_variables()));
}

void Evaluation::fit(const Lookup &lookup)
{
    const std::size_t threads = pool_.capped(to_size(lookup.options().get_int(Option::Threads)));
    team_.fit(program_.tape, threads);
    program_.hessian.fit(threads);
}

Request Evaluation::request(Index callback, const Point &point) noexcept
{
    const Callback &held = model_.callback(callback);
    return Request{
        .session = stream_.session(),
        .callback = callback,
        .thread = 0,
        .x = read_by(held, point.x, point_),
        .w = point.w,
        .v = read_by(held, point.v, direction_),
    };
}

void Evaluation::term_function(std::span<const Real> x, std::span<Real> f) const
{
    const Index n = model_.n_functions();
    const std::span<const Real> constants = model_.constants(0, n);
    const std::span<const LinearTerm> linear = model_.linear_terms(0, n);
    const std::span<const QuadraticTerm> quadratic = model_.quadratic_terms(0, n);
    const evaluation::Layout &layout = program_.layout;
    for (std::size_t k = 0; k < to_size(n); ++k)
    {
        f[k] = constants[k];
        for (Offset t = layout.starts.linear[k]; t < layout.starts.linear[k + 1]; ++t)
        {
            const LinearTerm &term = linear[to_size(t)];
            f[k] += term.coefficient * x[to_size(term.variable)];
        }
        for (Offset t = layout.starts.quadratic[k]; t < layout.starts.quadratic[k + 1]; ++t)
        {
            const QuadraticTerm &term = quadratic[to_size(t)];
            f[k] += term.coefficient * x[to_size(term.row)] * x[to_size(term.column)];
        }
    }
}

void Evaluation::term_jacobian(std::span<const Real> x, std::span<Real> g) const
{
    const evaluation::Layout &layout = program_.layout;
    std::ranges::copy(program_.linear, g.begin());
    const std::span<const QuadraticTerm> quadratic =
        model_.quadratic_terms(0, model_.n_functions());
    for (std::size_t t = 0; t < quadratic.size(); ++t)
    {
        const QuadraticTerm &term = quadratic[t];
        g[to_size(layout.slots.quadratic[2 * t])] += term.coefficient * x[to_size(term.column)];
        g[to_size(layout.slots.quadratic[2 * t + 1])] += term.coefficient * x[to_size(term.row)];
    }
}

void Evaluation::evaluate_function(const Checker &checker, const Point &point, std::span<Real> f)
{
    for (Index c = 0; c < model_.n_callbacks(); ++c)
    {
        const Callback &callback = model_.callback(c);
        const std::span<Real> out = std::span(out_).first(callback.covered.size());
        evaluate(checker, callback, Part::Function, request(c, point), out);
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            f[to_size(callback.covered[i])] += out[i];
        }
    }
}

void Evaluation::evaluate_jacobian(const Checker &checker, const Point &point, std::span<Real> g)
{
    const evaluation::Layout &layout = program_.layout;
    for (Index c = 0; c < model_.n_callbacks(); ++c)
    {
        const Callback &callback = model_.callback(c);
        const std::span<Real> out = std::span(out_).first(callback.jacobian_pairs.size());
        evaluate(checker, callback, Part::Jacobian, request(c, point), out);
        const std::size_t begin = to_size(layout.starts.callbacks[to_size(c)]);
        for (std::size_t p = 0; p < out.size(); ++p)
        {
            g[to_size(layout.slots.callbacks[begin + p])] += out[p];
        }
    }
}

}
