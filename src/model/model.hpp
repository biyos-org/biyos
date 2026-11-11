#pragma once

#include "biyos/model.h"
#include "core/index.hpp"
#include "expr/terms.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

enum class VariableKind
{
    Continuous = BIYOS_VAR_CONTINUOUS,
    Binary = BIYOS_VAR_BINARY,
    Integer = BIYOS_VAR_INTEGER,
    SemiContinuous = BIYOS_VAR_SEMICONTINUOUS,
    SemiInteger = BIYOS_VAR_SEMIINTEGER,
};

inline constexpr std::array kVariableKinds{
    VariableKind::Continuous,
    VariableKind::Binary,
    VariableKind::Integer,
    VariableKind::SemiContinuous,
    VariableKind::SemiInteger,
};

struct Variable
{
    VariableKind kind;
    double lower;
    double upper;
};

Variable widest(VariableKind kind) noexcept;

struct Constraint
{
    Index function;
    double lower;
    double upper;
};

void check_starts(std::span<const Offset> starts);
void check_shape(std::size_t functions, std::span<const Offset> starts, std::size_t terms);

class Model
{
  public:
    Index variable_count() const noexcept;
    const Variable &variable(Index index) const;
    std::span<const Variable> variables(Index first, Index count) const;

    void add_variables(std::span<const Variable> added);
    void set_variables(std::span<const Index> indices, std::span<const Variable> changed);

    Index function_count() const noexcept;
    std::span<const double> constants(Index first, Index count) const;
    std::span<const LinearTerm> terms(Index first, Index count) const;

    void check_room(std::size_t functions, Offset terms) const;
    void add_functions(std::span<const double> constants,
                       std::span<const Offset> starts,
                       std::vector<LinearTerm> terms);

    Index constraint_count() const noexcept;
    const Constraint &constraint(Index index) const;
    std::span<const Constraint> constraints(Index first, Index count) const;

    void add_constraints(std::span<const Constraint> added);
    void set_constraints(std::span<const Index> indices, std::span<const Constraint> changed);

  private:
    Offset begin_of(std::size_t function) const noexcept;
    void append(std::span<const double> constants,
                std::span<const Offset> starts,
                std::span<const LinearTerm> terms);

    std::vector<Variable> variables_;
    std::vector<double> constants_;
    std::vector<Offset> ends_;
    std::vector<LinearTerm> terms_;
    std::vector<Constraint> constraints_;
};

}
