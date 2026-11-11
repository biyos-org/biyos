#pragma once

#include "core/index.hpp"
#include "expr/node.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace biyos
{

class Graph
{
  public:
    Graph();

    Offset size() const noexcept;
    Operator op(Offset node) const noexcept;
    std::span<const Offset> children(Offset node) const noexcept;
    double constant(Offset node) const noexcept;
    Index variable(Offset node) const noexcept;
    std::span<const double> coefficients(Offset node) const noexcept;

    Offset add(Operator op, std::span<const Offset> children);
    Offset add(Operator op, double constant, std::span<const Offset> children);
    Offset add_variable(Index variable);
    Offset add_sum(double constant,
                   std::span<const Offset> children,
                   std::span<const double> coefficients);

  private:
    void check(Operator op, Payload payload, std::span<const Offset> children) const;
    Offset append(Operator op, std::uint64_t payload, std::span<const Offset> children);

    std::vector<Operator> ops_;
    std::vector<std::uint64_t> payloads_;
    std::vector<Offset> starts_;
    std::vector<Offset> children_;
    std::vector<double> coefficients_;
    std::vector<Offset> variable_nodes_;
};

}
