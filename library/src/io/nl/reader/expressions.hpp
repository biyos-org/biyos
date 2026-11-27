#pragma once

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "io/nl/operators.hpp"
#include "io/nl/source/source.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace biyos::nl
{

struct Operand
{
    Offset node;
    std::optional<Real> value;
};

class Expressions
{
  public:
    Expressions(Source &source, Model &model, Index variables, Index defined);

    Operand read();
    Offset node_of(const Operand &operand);
    bool has(Index variable) const noexcept;
    void define(Index variable, std::span<const LinearTerm> terms, const Operand &body);

  private:
    struct Frame
    {
        const Opcode *opcode;
        std::size_t first;
        std::size_t needed;
    };

    void take();
    void open(const Opcode &opcode);
    void close();
    std::size_t operand_count(const Opcode &opcode);
    Offset apply(const Opcode &opcode, std::span<const Operand> operands);
    void gather(std::span<const Operand> operands);
    std::span<const Real> ones(std::size_t n);
    Offset sum_of(std::span<const Operand> operands, std::span<const Real> weights);
    Offset power_of(const Operand &base, const Operand &exponent);
    Offset log10_of(const Operand &operand);
    Offset less_of(std::span<const Operand> operands);
    Offset node(Operator op, std::span<const Offset> children, std::span<const Real> values);
    Offset piecewise();
    Real piece();
    Offset variable_node();

    Source &source_;
    Model &model_;
    Index variables_;
    std::vector<Offset> defined_;
    std::vector<Frame> frames_;
    std::vector<Operand> operands_;
    std::vector<Offset> children_;
    std::vector<Real> values_;
    std::vector<Real> ones_;
    std::vector<Real> slopes_;
    std::vector<Real> breaks_;
};

}
