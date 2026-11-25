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
    check_point(x);
    take_weights(w);
    check_size("h", h.size(), hessian_pattern().rows.size());
    require_all(model_, Part::Hessian);
    program_.hessian.values(x, root_weights_, h);
    term_hessian(w, h);
    add_user_hessian(Checker(env_, lookup, "model"), x, w, h);
}

void Evaluation::product(const Lookup &lookup,
                         std::span<const Real> x,
                         std::span<const Real> w,
                         std::span<const Real> v,
                         std::span<Real> hv)
{
    const Timer timer(env_, tree_, "model", "product");
    check_point(x);
    take_weights(w);
    check_size("v", v.size(), x.size());
    check_size("hv", hv.size(), x.size());
    require_products(model_);
    program_.hessian.product(x, root_weights_, v, hv);
    term_product(w, v, hv);
    add_user_product(Checker(env_, lookup, "model"), x, w, v, hv);
}

void Evaluation::take_weights(std::span<const Real> w)
{
    check_size("w", w.size(), to_size(model_.n_functions()));
    for (std::size_t e = 0; e < root_weights_.size(); ++e)
    {
        root_weights_[e] = w[to_size(program_.rooted[e])];
    }
}

std::span<const Real> Evaluation::covered_weights(const Callback &callback, std::span<const Real> w)
{
    for (std::size_t i = 0; i < callback.covered.size(); ++i)
    {
        user_weights_[i] = w[to_size(callback.covered[i])];
    }
    return std::span(user_weights_).first(callback.covered.size());
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

void Evaluation::term_product(std::span<const Real> w,
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

void Evaluation::add_user_hessian(const Checker &checker,
                                  std::span<const Real> x,
                                  std::span<const Real> w,
                                  std::span<Real> h)
{
    const std::span<const Offset> declared = program_.hessian.declared();
    for (Index c = 0; c < model_.n_callbacks(); ++c)
    {
        const Callback &callback = model_.callback(c);
        const std::span<Real> out = std::span(out_).first(callback.hessian_pairs.size());
        run(checker, callback, Part::Hessian, request_of(c, x, covered_weights(callback, w)), out);
        const std::size_t begin = to_size(program_.entries.starts[to_size(c)]);
        for (std::size_t p = 0; p < out.size(); ++p)
        {
            h[to_size(declared[begin + p])] += out[p];
        }
    }
}

void Evaluation::add_user_product(const Checker &checker,
                                  std::span<const Real> x,
                                  std::span<const Real> w,
                                  std::span<const Real> v,
                                  std::span<Real> hv)
{
    for (Index c = 0; c < model_.n_callbacks(); ++c)
    {
        const Callback &callback = model_.callback(c);
        const Request request = request_of(c, x, covered_weights(callback, w), v);
        if (has(callback, Part::Product))
        {
            const std::span<Real> out = std::span(out_).first(hv.size());
            run(checker, callback, Part::Product, request, out);
            for (std::size_t i = 0; i < out.size(); ++i)
            {
                hv[i] += out[i];
            }
            continue;
        }
        const std::span<Real> out = std::span(out_).first(callback.hessian_pairs.size());
        run(checker, callback, Part::Hessian, request, out);
        add_pairs_product(callback, out, v, hv);
    }
}

void Evaluation::add_pairs_product(const Callback &callback,
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
