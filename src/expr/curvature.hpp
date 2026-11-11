#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/interval.hpp"
#include "expr/shape.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

class Shapes
{
  public:
    explicit Shapes(const Graph &graph);

    void classify(const Graph &graph, std::span<const Interval> nodes);
    Curvature curvature(Offset node) const noexcept;
    Monotonicity monotonicity(Offset node, std::size_t position) const noexcept;

  private:
    void check(const Graph &graph, std::span<const Interval> nodes) const;
    std::span<Argument> arguments_of(Offset node) noexcept;
    Curvature shape_of(const Graph &graph, std::span<const Interval> nodes, Offset node) noexcept;
    Curvature composed(Curvature outer,
                       std::span<const Offset> children,
                       std::span<const Argument> arguments) const noexcept;

    std::vector<Offset> starts_;
    std::vector<Curvature> curvatures_;
    std::vector<Argument> arguments_;
};

}
