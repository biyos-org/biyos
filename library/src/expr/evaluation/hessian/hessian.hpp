#pragma once

#include "core/env/pool.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/timing/timer.hpp"
#include "expr/evaluation/hessian/coloring.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/evaluation/hessian/pattern.hpp"
#include "expr/evaluation/hessian/pushing.hpp"
#include "expr/evaluation/hessian/sweep.hpp"
#include "expr/graph/graph.hpp"
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
            Timing timing,
            std::size_t lanes = kLanes,
            Offset budget = kBudget);

    const Lower &pattern() const noexcept;
    std::span<const Offset> declared() const noexcept;
    bool colored(Offset element) const noexcept;
    bool pushed(Offset element) const noexcept;
    void widen(Index columns);
    void fit(std::size_t threads);
    void values(std::span<const Real> point,
                std::span<const Real> weights,
                const Tolerances &tolerances,
                std::span<Real> out,
                Pool &pool,
                Timing timing);
    void product(std::span<const Real> point,
                 std::span<const Real> weights,
                 std::span<const Real> direction,
                 const Tolerances &tolerances,
                 std::span<Real> out,
                 Pool &pool);

  private:
    struct Products;

    void color(Timing timing);
    void choose(Offset e, Shape shape, Stream stream);
    std::size_t directions(Offset e) const noexcept;
    void check(std::span<const Real> point, std::span<const Real> weights) const;
    void weigh(std::span<const Real> weights) noexcept;
    void fill_blocks(std::size_t chunk,
                     std::size_t worker,
                     std::span<const Real> point,
                     const Tolerances &tolerances) noexcept;
    void fill_pushed(std::size_t k,
                     std::size_t worker,
                     std::span<const Real> point,
                     const Tolerances &tolerances) noexcept;
    void push_elements(std::span<const Real> point,
                       const Tolerances &tolerances,
                       Pool &pool,
                       Timing timing);
    void fill_products(std::size_t chunk,
                       std::size_t worker,
                       std::span<const Real> point,
                       std::span<const Real> direction,
                       const Tolerances &tolerances) noexcept;
    void write_block(Offset e,
                     Real scale,
                     std::span<const Real> point,
                     const Tolerances &tolerances,
                     std::vector<Pass> &passes) noexcept;
    void write_colored(Offset e,
                       Real scale,
                       std::span<const Real> point,
                       const Tolerances &tolerances,
                       std::vector<Pass> &passes) noexcept;
    void write_product(Offset e,
                       Real scale,
                       std::span<const Real> point,
                       std::span<const Real> direction,
                       const Tolerances &tolerances,
                       Pass &pass) noexcept;

    std::size_t roots_;
    Elements elements_;
    Tape tape_;
    std::vector<Shape> shapes_;
    Blocks blocks_;
    Lower lower_;
    Map map_;
    std::vector<Offset> places_;
    std::vector<Coloring> colorings_;
    std::vector<Offset> pushed_;
    std::vector<Elimination> eliminations_;
    std::vector<Lists> lists_;
    std::vector<std::size_t> widths_;
    std::vector<std::size_t> classes_;
    std::vector<std::vector<Pass>> workers_;
    std::vector<Offset> chunks_;
    std::vector<Real> scales_;
    std::vector<Real> parts_;
};

}
