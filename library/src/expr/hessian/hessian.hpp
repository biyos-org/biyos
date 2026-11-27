#pragma once

#include "core/env/pool.hpp"
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
    void fit(std::size_t threads);
    void values(std::span<const Real> point,
                std::span<const Real> weights,
                std::span<Real> out,
                Pool &pool);
    void product(std::span<const Real> point,
                 std::span<const Real> weights,
                 std::span<const Real> direction,
                 std::span<Real> out,
                 Pool &pool);

  private:
    void check(std::span<const Real> point, std::span<const Real> weights) const;
    void spread(std::span<const Real> point, std::size_t worker, std::size_t passes) noexcept;
    void weigh(std::span<const Real> weights) noexcept;
    void fill_blocks(std::size_t chunk, std::size_t worker) noexcept;
    void fill_products(std::size_t chunk,
                       std::size_t worker,
                       std::span<const Real> direction) noexcept;
    void write_block(Offset e, Real scale, std::vector<Pass> &passes) noexcept;
    void write_product(Offset e, Real scale, std::span<const Real> direction, Pass &pass) noexcept;

    std::size_t roots_;
    Elements elements_;
    Tape tape_;
    Lower lower_;
    Map map_;
    std::vector<std::size_t> widths_;
    std::vector<std::size_t> classes_;
    std::vector<std::vector<Pass>> workers_;
    std::vector<Offset> chunks_;
    std::vector<Real> scales_;
    std::vector<Real> parts_;
};

}
