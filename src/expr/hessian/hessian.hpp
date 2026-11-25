#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/hessian/elements.hpp"
#include "expr/hessian/sweep.hpp"
#include "expr/tape/tape.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

constexpr std::size_t kLanes = 16;

class Hessian
{
  public:
    Hessian(const Graph &graph,
            std::span<const Offset> roots,
            std::span<const Entry> declared,
            std::size_t lanes = kLanes,
            Offset budget = kBudget);

    const Lower &pattern() const noexcept;
    std::span<const Offset> declared() const noexcept;
    void widen(Index columns);
    void values(std::span<const Real> point, std::span<const Real> weights, std::span<Real> out);
    void product(std::span<const Real> point,
                 std::span<const Real> weights,
                 std::span<const Real> direction,
                 std::span<Real> out);

  private:
    void check(std::span<const Real> point, std::span<const Real> weights) const;
    void add_block(Offset e, Real scale, std::span<Real> out) noexcept;
    void add_product(Offset e,
                     Real scale,
                     std::span<const Real> direction,
                     std::span<Real> out) noexcept;

    std::size_t roots_;
    Elements elements_;
    Tape tape_;
    Lower lower_;
    Map map_;
    std::vector<Pass> passes_;
    std::vector<std::size_t> classes_;
};

}
