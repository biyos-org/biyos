#pragma once

#include "biyos/biyos.h"
#include "core/check.hpp"
#include "core/env.hpp"
#include "core/numbers.hpp"
#include "core/options/lookup.hpp"
#include "core/timing/timer.hpp"
#include "expr/derivatives/reverse.hpp"
#include "expr/derivatives/value.hpp"
#include "expr/hessian/elements.hpp"
#include "expr/hessian/hessian.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/tape.hpp"
#include "model/evaluation/layout.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace biyos
{

struct Point
{
    std::span<const Real> x;
    std::span<const Real> w;
    std::span<const Real> v;
};

struct Program
{
    std::vector<Index> rooted;
    std::vector<Offset> roots;
    Tape tape;
    JacobianLayout layout;
    std::vector<Real> linear;
    Entries entries;
    Hessian hessian;
};

class Evaluation
{
  public:
    Evaluation(Env &env, const Model &model, const BIYOS_model *handle = nullptr);

    const Pattern &jacobian_pattern() const noexcept;
    const Lower &hessian_pattern() const noexcept;

    void values(const Lookup &lookup, std::span<const Real> x, std::span<Real> f);
    void jacobian(const Lookup &lookup, std::span<const Real> x, std::span<Real> g);
    void hessian(const Lookup &lookup, const Point &point, std::span<Real> h);
    void product(const Lookup &lookup, const Point &point, std::span<Real> hv);

  private:
    static void check(std::string_view argument, std::size_t size, std::size_t wanted);
    void check(std::span<const Real> x) const;
    void weigh(std::span<const Real> w);
    Request request(Index callback, const Point &point) const noexcept;
    std::span<const Real> covered(const Callback &callback, std::span<const Real> w);
    void term_function(std::span<const Real> x, std::span<Real> f) const;
    void term_jacobian(std::span<const Real> x, std::span<Real> g) const;
    void evaluate_function(const Checker &checker, const Point &point, std::span<Real> f);
    void evaluate_jacobian(const Checker &checker, const Point &point, std::span<Real> g);
    void term_hessian(std::span<const Real> w, std::span<Real> h) const;
    void term_hessian_product(const Point &point, std::span<Real> hv) const;
    void evaluate_hessian(const Checker &checker, const Point &point, std::span<Real> h);
    void evaluate_hessian_product(Checker checker, const Point &point, std::span<Real> hv);
    void multiply(Index c, std::span<const Real> v, std::span<Real> hv) const noexcept;

    Env &env_;
    const Model &model_;
    const BIYOS_model *handle_;
    Program program_;
    Workspace workspace_;
    Adjoints adjoints_;
    std::vector<Real> values_;
    std::vector<Real> gradients_;
    std::vector<Real> out_;
    std::vector<Real> weights_;
    std::vector<Real> covered_;
    TimingTree tree_;
};

}
