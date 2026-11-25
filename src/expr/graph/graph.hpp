#pragma once

#include "core/numbers.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/points.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace biyos
{

class Graph
{
  public:
    struct Mark
    {
        Offset nodes;
        std::size_t coefficients;
        std::size_t variables;
        Offset pwls;

        bool operator==(const Mark &) const = default;
    };

    Graph();
    explicit Graph(Points points);

    Offset size() const noexcept;
    Operator op(Offset node) const noexcept;
    std::span<const Offset> children(Offset node) const noexcept;
    double constant(Offset node) const noexcept;
    Index variable(Offset node) const noexcept;
    std::span<const double> coefficients(Offset node) const noexcept;
    Offset pwl(Offset node) const noexcept;
    Offset node_of(Index variable) const noexcept;
    const Points &points() const noexcept;

    Offset add(Operator op, std::span<const Offset> children);
    Offset add(Operator op, double constant, std::span<const Offset> children);
    Offset add_variable(Index variable);
    Offset add_sum(double constant,
                   std::span<const Offset> children,
                   std::span<const double> coefficients);
    Offset add_pwl(std::span<const double> x, std::span<const double> y);
    Offset add_piecewise(Offset pwl, std::span<const Offset> children);

    void renumber_variables(std::span<const Index> map) noexcept;

    Mark mark() const noexcept;
    void truncate(const Mark &mark) noexcept;

  private:
    void check(Operator op, Payload payload, std::span<const Offset> children) const;
    Offset append(Operator op, std::uint64_t payload, std::span<const Offset> children);

    std::vector<Operator> operators_;
    std::vector<std::uint64_t> payloads_;
    std::vector<Offset> starts_;
    std::vector<Offset> children_;
    std::vector<double> coefficients_;
    std::vector<Offset> variable_nodes_;
    Points points_;
};

}
