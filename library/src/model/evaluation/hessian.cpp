#include "model/evaluation/evaluation.hpp"

#include "core/check.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/options/lookup.hpp"
#include "core/timing/timer.hpp"
#include "expr/forms/terms.hpp"
#include "model/evaluation/callbacks.hpp"
#include "model/evaluation/layout.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>

namespace biyos
{

using evaluation::evaluate;
using evaluation::gather;
using evaluation::has;
using evaluation::Part;
using evaluation::require;
using evaluation::scatter;

namespace
{

void require_products(const Model &model)
{
    for (Index c = 0; c < model.n_callbacks(); ++c)
    {
        const Callback &callback = model.callback(c);
        if (!has(callback, Part::Product))
        {
            require(callback, c, Part::Hessian);
        }
    }
}

}

void Evaluation::hessian(const Lookup &lookup, const Point &point, std::span<Real> h)
{
    const Timer timer(stream_, tree_, kModule, "hessian");
    check(point.x);
    weigh(point.w);
    check("h", h.size(), hessian_pattern().rows.size());
    require(model_, Part::Hessian);
    fit(lookup);
    program_.hessian.values(point.x, weights_, h, pool_);
    term_hessian(point.w, h);
    evaluate_hessian(Checker(stream_, lookup, kModule), point, h);
}

void Evaluation::product(const Lookup &lookup, const Point &point, std::span<Real> hv)
{
    const Timer timer(stream_, tree_, kModule, "product");
    check(point.x);
    weigh(point.w);
    check("v", point.v.size(), point.x.size());
    check("hv", hv.size(), point.x.size());
    require_products(model_);
    fit(lookup);
    program_.hessian.product(point.x, weights_, point.v, hv, pool_);
    term_hessian_product(point, hv);
    evaluate_hessian_product(Checker(stream_, lookup, kModule), point, hv);
}

void Evaluation::weigh(std::span<const Real> w)
{
    check("w", w.size(), to_size(model_.n_functions()));
    for (std::size_t e = 0; e < weights_.size(); ++e)
    {
        weights_[e] = w[to_size(program_.rooted[e])];
    }
}

std::span<const Real> Evaluation::covered(const Callback &callback, std::span<const Real> w)
{
    return gather(callback.covered, w, covered_);
}

void Evaluation::term_hessian(std::span<const Real> w, std::span<Real> h) const
{
    const std::span<const QuadraticTerm> quadratic =
        model_.quadratic_terms(0, model_.n_functions());
    const std::span<const Offset> declared = program_.hessian.declared();
    const evaluation::Layout &layout = program_.layout;
    for (std::size_t k = 0; k < w.size(); ++k)
    {
        for (Offset t = layout.starts.quadratic[k]; t < layout.starts.quadratic[k + 1]; ++t)
        {
            const QuadraticTerm &term = quadratic[to_size(t)];
            const Real scale = term.row == term.column ? 2.0 : 1.0;
            h[to_size(declared[to_size(t)])] += scale * w[k] * term.coefficient;
        }
    }
}

void Evaluation::term_hessian_product(const Point &point, std::span<Real> hv) const
{
    const std::span<const QuadraticTerm> quadratic =
        model_.quadratic_terms(0, model_.n_functions());
    const evaluation::Layout &layout = program_.layout;
    for (std::size_t k = 0; k < point.w.size(); ++k)
    {
        for (Offset t = layout.starts.quadratic[k]; t < layout.starts.quadratic[k + 1]; ++t)
        {
            const QuadraticTerm &term = quadratic[to_size(t)];
            const Real scale = point.w[k] * term.coefficient;
            hv[to_size(term.row)] += scale * point.v[to_size(term.column)];
            hv[to_size(term.column)] += scale * point.v[to_size(term.row)];
        }
    }
}

void Evaluation::evaluate_hessian(const Checker &checker, const Point &point, std::span<Real> h)
{
    const std::span<const Offset> declared = program_.hessian.declared();
    for (Index c = 0; c < model_.n_callbacks(); ++c)
    {
        const Callback &callback = model_.callback(c);
        const std::span<Real> out = std::span(out_).first(callback.hessian_pairs.size());
        const std::span<const Real> w = covered(callback, point.w);
        const Request arguments = request(c, {.x = point.x, .w = w, .v = {}});
        evaluate(checker, callback, Part::Hessian, arguments, out);
        const std::size_t begin = to_size(program_.entries.starts[to_size(c)]);
        for (std::size_t p = 0; p < out.size(); ++p)
        {
            h[to_size(declared[begin + p])] += out[p];
        }
    }
}

void Evaluation::evaluate_hessian_product(Checker checker, const Point &point, std::span<Real> hv)
{
    for (Index c = 0; c < model_.n_callbacks(); ++c)
    {
        const Callback &callback = model_.callback(c);
        const std::span<const Real> w = covered(callback, point.w);
        if (has(callback, Part::Product))
        {
            const Request arguments = request(c, {.x = point.x, .w = w, .v = point.v});
            const std::span<Real> out = std::span(out_).first(arguments.v.size());
            evaluate(checker, callback, Part::Product, arguments, out);
            scatter(callback, out, hv);
            continue;
        }
        const Request arguments = request(c, {.x = point.x, .w = w, .v = {}});
        const std::span<Real> out = std::span(out_).first(callback.hessian_pairs.size());
        evaluate(checker, callback, Part::Hessian, arguments, out);
        multiply(c, point.v, hv);
    }
}

void Evaluation::multiply(Index c, std::span<const Real> v, std::span<Real> hv) const noexcept
{
    const std::vector<IndexPair> &pairs = model_.callback(c).hessian_pairs;
    for (std::size_t p = 0; p < pairs.size(); ++p)
    {
        const IndexPair &pair = pairs[p];
        hv[to_size(pair.row)] += out_[p] * v[to_size(pair.column)];
        if (pair.row != pair.column)
        {
            hv[to_size(pair.column)] += out_[p] * v[to_size(pair.row)];
        }
    }
}

}
