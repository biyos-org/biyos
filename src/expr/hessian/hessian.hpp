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
    void values(std::span<const double> point,
                std::span<const double> weights,
                std::span<double> out);
    void product(std::span<const double> point,
                 std::span<const double> weights,
                 std::span<const double> direction,
                 std::span<double> out);

  private:
    void check(std::span<const double> point, std::span<const double> weights) const;
    void add_block(Offset e, double scale, std::span<double> out) noexcept;
    void add_product(Offset e,
                     double scale,
                     std::span<const double> direction,
                     std::span<double> out) noexcept;

    std::size_t roots_;
    Elements elements_;
    Tape tape_;
    Lower lower_;
    Map map_;
    std::vector<Pass> passes_;
    std::vector<std::size_t> classes_;
};

}
