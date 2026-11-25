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

struct Program
{
    std::vector<Index> rooted;
    std::vector<Offset> roots;
    Tape tape;
    JacobianLayout layout;
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
    void hessian(const Lookup &lookup,
                 std::span<const Real> x,
                 std::span<const Real> w,
                 std::span<Real> h);
    void product(const Lookup &lookup,
                 std::span<const Real> x,
                 std::span<const Real> w,
                 std::span<const Real> v,
                 std::span<Real> hv);

  private:
    static void check_size(std::string_view argument, std::size_t size, std::size_t wanted);
    void check_point(std::span<const Real> x) const;
    void take_weights(std::span<const Real> w);
    Request request_of(Index callback,
                       std::span<const Real> x,
                       std::span<const Real> w = {},
                       std::span<const Real> v = {}) const noexcept;
    std::span<const Real> covered_weights(const Callback &callback, std::span<const Real> w);
    void term_values(std::span<const Real> x, std::span<Real> f) const;
    void term_gradients(std::span<const Real> x, std::span<Real> g) const;
    void add_user_values(const Checker &checker, std::span<const Real> x, std::span<Real> f);
    void add_user_gradients(const Checker &checker, std::span<const Real> x, std::span<Real> g);
    void term_hessian(std::span<const Real> w, std::span<Real> h) const;
    void term_product(std::span<const Real> w, std::span<const Real> v, std::span<Real> hv) const;
    void add_user_hessian(const Checker &checker,
                          std::span<const Real> x,
                          std::span<const Real> w,
                          std::span<Real> h);
    void add_user_product(const Checker &checker,
                          std::span<const Real> x,
                          std::span<const Real> w,
                          std::span<const Real> v,
                          std::span<Real> hv);
    void add_pairs_product(const Callback &callback,
                           std::span<const Real> out,
                           std::span<const Real> v,
                           std::span<Real> hv) const noexcept;

    Env &env_;
    const Model &model_;
    const BIYOS_model *handle_;
    Program program_;
    Workspace workspace_;
    Adjoints adjoints_;
    std::vector<Real> tape_values_;
    std::vector<Real> tape_gradients_;
    std::vector<Real> out_;
    std::vector<Real> root_weights_;
    std::vector<Real> user_weights_;
    TimingTree tree_;
};

}
