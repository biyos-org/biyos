#include "model/evaluation/evaluation.hpp"

#include "core/check.hpp"
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

void Evaluation::hessian(const Lookup &lookup,
                         std::span<const Real> x,
                         std::span<const Real> w,
                         std::span<Real> h)
{
    const Timer timer(env_, tree_, "model", "hessian");
    check(x);
    weigh(w);
    check("h", h.size(), hessian_pattern().rows.size());
    require(model_, Part::Hessian);
    program_.hessian.values(x, weights_, h);
    term_hessian(w, h);
    evaluate_hessian(Checker(env_, lookup, "model"), x, w, h);
}

void Evaluation::product(const Lookup &lookup,
                         std::span<const Real> x,
                         std::span<const Real> w,
                         std::span<const Real> v,
                         std::span<Real> hv)
{
    const Timer timer(env_, tree_, "model", "product");
    check(x);
    weigh(w);
    check("v", v.size(), x.size());
    check("hv", hv.size(), x.size());
    require_products(model_);
    program_.hessian.product(x, weights_, v, hv);
    term_hessian_product(w, v, hv);
    evaluate_hessian_product(Checker(env_, lookup, "model"), x, w, v, hv);
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
    for (std::size_t i = 0; i < callback.covered.size(); ++i)
    {
        covered_[i] = w[to_size(callback.covered[i])];
    }
    return std::span(covered_).first(callback.covered.size());
}

void Evaluation::term_hessian(std::span<const Real> w, std::span<Real> h) const
{
    const std::span<const QuadraticTerm> quadratic =
        model_.quadratic_terms(0, model_.n_functions());
    const std::span<const Offset> declared = program_.hessian.declared();
    const JacobianLayout &layout = program_.layout;
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

void Evaluation::term_hessian_product(std::span<const Real> w,
                                      std::span<const Real> v,
                                      std::span<Real> hv) const
{
    const std::span<const QuadraticTerm> quadratic =
        model_.quadratic_terms(0, model_.n_functions());
    const JacobianLayout &layout = program_.layout;
    for (std::size_t k = 0; k < w.size(); ++k)
    {
        for (Offset t = layout.starts.quadratic[k]; t < layout.starts.quadratic[k + 1]; ++t)
        {
            const QuadraticTerm &term = quadratic[to_size(t)];
            const Real scale = w[k] * term.coefficient;
            hv[to_size(term.row)] += scale * v[to_size(term.column)];
            hv[to_size(term.column)] += scale * v[to_size(term.row)];
        }
    }
}

void Evaluation::evaluate_hessian(const Checker &checker,
                                  std::span<const Real> x,
                                  std::span<const Real> w,
                                  std::span<Real> h)
{
    const std::span<const Offset> declared = program_.hessian.declared();
    for (Index c = 0; c < model_.n_callbacks(); ++c)
    {
        const Callback &callback = model_.callback(c);
        const std::span<Real> out = std::span(out_).first(callback.hessian_pairs.size());
        evaluate(checker, callback, Part::Hessian, request(c, x, covered(callback, w)), out);
        const std::size_t begin = to_size(program_.entries.starts[to_size(c)]);
        for (std::size_t p = 0; p < out.size(); ++p)
        {
            h[to_size(declared[begin + p])] += out[p];
        }
    }
}

void Evaluation::evaluate_hessian_product(const Checker &checker,
                                          std::span<const Real> x,
                                          std::span<const Real> w,
                                          std::span<const Real> v,
                                          std::span<Real> hv)
{
    for (Index c = 0; c < model_.n_callbacks(); ++c)
    {
        const Callback &callback = model_.callback(c);
        const Request arguments = request(c, x, covered(callback, w), v);
        if (has(callback, Part::Product))
        {
            const std::span<Real> out = std::span(out_).first(hv.size());
            evaluate(checker, callback, Part::Product, arguments, out);
            for (std::size_t i = 0; i < out.size(); ++i)
            {
                hv[i] += out[i];
            }
            continue;
        }
        const std::span<Real> out = std::span(out_).first(callback.hessian_pairs.size());
        evaluate(checker, callback, Part::Hessian, arguments, out);
        multiply(callback, out, v, hv);
    }
}

void Evaluation::multiply(const Callback &callback,
                          std::span<const Real> out,
                          std::span<const Real> v,
                          std::span<Real> hv) const noexcept
{
    for (std::size_t p = 0; p < out.size(); ++p)
    {
        const IndexPair &pair = callback.hessian_pairs[p];
        hv[to_size(pair.row)] += out[p] * v[to_size(pair.column)];
        if (pair.row != pair.column)
        {
            hv[to_size(pair.column)] += out[p] * v[to_size(pair.row)];
        }
    }
}

}
