#include "model/evaluation/evaluation.hpp"

#include "core/check.hpp"
#include "core/numbers.hpp"
#include "core/options/lookup.hpp"
#include "core/timing/timer.hpp"
#include "expr/forms/terms.hpp"
#include "model/evaluation/maps.hpp"
#include "model/evaluation/user.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>

namespace biyos
{

namespace
{

std::size_t at(Offset offset) noexcept
{
    return static_cast<std::size_t>(offset);
}

std::size_t count_of(Index count) noexcept
{
    return static_cast<std::size_t>(count);
}

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
                         std::span<const double> x,
                         std::span<const double> w,
                         std::span<double> h)
{
    const Timer timer(env_, tree_, "model", "hessian");
    check_point(x);
    take_weights(w);
    check_size("h", h.size(), hessian_pattern().rows.size());
    require_all(model_, Part::Hessian);
    compiled_.hessian.values(x, root_weights_, h);
    term_hessian(w, h);
    add_user_hessian(Checker(env_, lookup, "model"), x, w, h);
}

void Evaluation::product(const Lookup &lookup,
                         std::span<const double> x,
                         std::span<const double> w,
                         std::span<const double> v,
                         std::span<double> hv)
{
    const Timer timer(env_, tree_, "model", "product");
    check_point(x);
    take_weights(w);
    check_size("v", v.size(), x.size());
    check_size("hv", hv.size(), x.size());
    require_products(model_);
    compiled_.hessian.product(x, root_weights_, v, hv);
    term_product(w, v, hv);
    add_user_product(Checker(env_, lookup, "model"), x, w, v, hv);
}

void Evaluation::take_weights(std::span<const double> w)
{
    check_size("w", w.size(), count_of(model_.n_functions()));
    for (std::size_t e = 0; e < root_weights_.size(); ++e)
    {
        root_weights_[e] = w[count_of(compiled_.rooted[e])];
    }
}

std::span<const double> Evaluation::covered_weights(const Callback &callback,
                                                    std::span<const double> w)
{
    for (std::size_t i = 0; i < callback.covered.size(); ++i)
    {
        user_weights_[i] = w[count_of(callback.covered[i])];
    }
    return std::span(user_weights_).first(callback.covered.size());
}

void Evaluation::term_hessian(std::span<const double> w, std::span<double> h) const
{
    const std::span<const QuadraticTerm> quadratic =
        model_.quadratic_terms(0, model_.n_functions());
    const std::span<const Offset> declared = compiled_.hessian.declared();
    const Slots &slots = compiled_.slots;
    for (std::size_t k = 0; k < w.size(); ++k)
    {
        for (Offset t = slots.quadratic_starts[k]; t < slots.quadratic_starts[k + 1]; ++t)
        {
            const QuadraticTerm &term = quadratic[at(t)];
            const double scale = term.row == term.column ? 2.0 : 1.0;
            h[at(declared[at(t)])] += scale * w[k] * term.coefficient;
        }
    }
}

void Evaluation::term_product(std::span<const double> w,
                              std::span<const double> v,
                              std::span<double> hv) const
{
    const std::span<const QuadraticTerm> quadratic =
        model_.quadratic_terms(0, model_.n_functions());
    const Slots &slots = compiled_.slots;
    for (std::size_t k = 0; k < w.size(); ++k)
    {
        for (Offset t = slots.quadratic_starts[k]; t < slots.quadratic_starts[k + 1]; ++t)
        {
            const QuadraticTerm &term = quadratic[at(t)];
            const double scale = w[k] * term.coefficient;
            hv[count_of(term.row)] += scale * v[count_of(term.column)];
            hv[count_of(term.column)] += scale * v[count_of(term.row)];
        }
    }
}

void Evaluation::add_user_hessian(const Checker &checker,
                                  std::span<const double> x,
                                  std::span<const double> w,
                                  std::span<double> h)
{
    const std::span<const Offset> declared = compiled_.hessian.declared();
    for (Index c = 0; c < model_.n_callbacks(); ++c)
    {
        const Callback &callback = model_.callback(c);
        const std::span<double> out = std::span(out_).first(callback.hessian_pairs.size());
        run(checker, callback, Part::Hessian, request_of(c, x, covered_weights(callback, w)), out);
        const std::size_t begin = at(compiled_.declared.callback_starts[count_of(c)]);
        for (std::size_t p = 0; p < out.size(); ++p)
        {
            h[at(declared[begin + p])] += out[p];
        }
    }
}

void Evaluation::add_user_product(const Checker &checker,
                                  std::span<const double> x,
                                  std::span<const double> w,
                                  std::span<const double> v,
                                  std::span<double> hv)
{
    for (Index c = 0; c < model_.n_callbacks(); ++c)
    {
        const Callback &callback = model_.callback(c);
        const Request request = request_of(c, x, covered_weights(callback, w), v);
        if (has(callback, Part::Product))
        {
            const std::span<double> out = std::span(out_).first(hv.size());
            run(checker, callback, Part::Product, request, out);
            for (std::size_t i = 0; i < out.size(); ++i)
            {
                hv[i] += out[i];
            }
            continue;
        }
        const std::span<double> out = std::span(out_).first(callback.hessian_pairs.size());
        run(checker, callback, Part::Hessian, request, out);
        add_pairs_product(callback, out, v, hv);
    }
}

void Evaluation::add_pairs_product(const Callback &callback,
                                   std::span<const double> out,
                                   std::span<const double> v,
                                   std::span<double> hv) const noexcept
{
    for (std::size_t p = 0; p < out.size(); ++p)
    {
        const IndexPair &pair = callback.hessian_pairs[p];
        hv[count_of(pair.row)] += out[p] * v[count_of(pair.column)];
        if (pair.row != pair.column)
        {
            hv[count_of(pair.column)] += out[p] * v[count_of(pair.row)];
        }
    }
}

}
