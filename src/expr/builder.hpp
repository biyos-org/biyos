#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

class Builder
{
  public:
    explicit Builder(Graph &graph);

    void constant(double value);
    void variable(Index variable);
    void node(Offset id);
    void sum(double constant, std::span<const double> coefficients);
    void product(double coefficient, Offset count);
    void power(double exponent);
    void sign_power(double exponent);
    void apply(Operator op, Offset count);
    Offset finish();

  private:
    struct Operand
    {
        Offset node;
        Operator op;
        double constant;
        Offset first;
        Offset count;
    };

    void raise(Operator op, double exponent);
    void push(Offset node);
    std::size_t take(Offset count) const;
    Offset resolve(std::size_t position);
    Offset append(const Operand &operand);
    void collect(std::size_t base);
    bool merges(const Operand &operand, double coefficient, double constant) const;
    void add_term(std::size_t position, double coefficient, double &constant);
    void add_factor(std::size_t position, double &coefficient);
    void pop(std::size_t base);
    void replace(std::size_t base, Operator op, double constant);

    Graph &graph_;
    std::vector<Operand> stack_;
    std::vector<Offset> terms_;
    std::vector<double> weights_;
    std::vector<Offset> next_terms_;
    std::vector<double> next_weights_;
};

}
