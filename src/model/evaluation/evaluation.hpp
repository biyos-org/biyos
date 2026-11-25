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
#include "model/evaluation/maps.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace biyos
{

struct Compiled
{
    std::vector<Index> rooted;
    std::vector<Offset> roots;
    Tape tape;
    Slots slots;
    Declared declared;
    Hessian hessian;
};

class Evaluation
{
  public:
    Evaluation(Env &env, const Model &model, const BIYOS_model *handle = nullptr);

    const Pattern &jacobian_pattern() const noexcept;
    const Lower &hessian_pattern() const noexcept;

    void values(const Lookup &lookup, std::span<const double> x, std::span<double> f);
    void jacobian(const Lookup &lookup, std::span<const double> x, std::span<double> g);
    void hessian(const Lookup &lookup,
                 std::span<const double> x,
                 std::span<const double> w,
                 std::span<double> h);
    void product(const Lookup &lookup,
                 std::span<const double> x,
                 std::span<const double> w,
                 std::span<const double> v,
                 std::span<double> hv);

  private:
    static void check_size(std::string_view argument, std::size_t size, std::size_t wanted);
    void check_point(std::span<const double> x) const;
    void take_weights(std::span<const double> w);
    Request request_of(Index callback,
                       std::span<const double> x,
                       std::span<const double> w = {},
                       std::span<const double> v = {}) const noexcept;
    std::span<const double> covered_weights(const Callback &callback, std::span<const double> w);
    void term_values(std::span<const double> x, std::span<double> f) const;
    void term_gradients(std::span<const double> x, std::span<double> g) const;
    void add_user_values(const Checker &checker, std::span<const double> x, std::span<double> f);
    void add_user_gradients(const Checker &checker, std::span<const double> x, std::span<double> g);
    void term_hessian(std::span<const double> w, std::span<double> h) const;
    void term_product(std::span<const double> w,
                      std::span<const double> v,
                      std::span<double> hv) const;
    void add_user_hessian(const Checker &checker,
                          std::span<const double> x,
                          std::span<const double> w,
                          std::span<double> h);
    void add_user_product(const Checker &checker,
                          std::span<const double> x,
                          std::span<const double> w,
                          std::span<const double> v,
                          std::span<double> hv);
    void add_pairs_product(const Callback &callback,
                           std::span<const double> out,
                           std::span<const double> v,
                           std::span<double> hv) const noexcept;

    Env &env_;
    const Model &model_;
    const BIYOS_model *handle_;
    Compiled compiled_;
    Workspace workspace_;
    Adjoints adjoints_;
    std::vector<double> tape_values_;
    std::vector<double> tape_gradients_;
    std::vector<double> out_;
    std::vector<double> root_weights_;
    std::vector<double> user_weights_;
    TimingTree tree_;
};

}
