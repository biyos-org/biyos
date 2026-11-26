#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

class Builder
{
  public:
    explicit Builder(Graph &graph);

    void constant(Real value);
    void variable(Index variable);
    void node(Offset id);
    void sum(Real constant, std::span<const Real> coefficients);
    void product(Real coefficient, Offset n);
    void power(Real exponent);
    void sign_power(Real exponent);
    void piecewise(Offset pwl);
    void apply(Operator op, Offset n);
    Offset keep();
    Offset finish();

  private:
    struct Operand
    {
        Offset node;
        Operator op;
        Real constant;
        Offset first;
        Offset count;
    };

    void raise(Operator op, Real exponent);
    void push(Offset node);
    std::size_t take(Offset n) const;
    Offset resolve(std::size_t position);
    Offset append(const Operand &operand);
    void collect(std::size_t base);
    bool merges(const Operand &operand, Real coefficient, Real constant) const;
    void add_term(std::size_t position, Real coefficient, Real &constant);
    void add_factor(std::size_t position, Real &coefficient);
    void pop(std::size_t base);
    void replace(std::size_t base, Operator op, Real constant);

    Graph &graph_;
    std::vector<Operand> stack_;
    std::vector<Offset> terms_;
    std::vector<Real> weights_;
    std::vector<Offset> next_terms_;
    std::vector<Real> next_weights_;
};

}
