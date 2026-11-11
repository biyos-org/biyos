#pragma once

#include "biyos/model.h"
#include "core/index.hpp"

#include <array>
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
    VariableKind::Continuous,     VariableKind::Binary,      VariableKind::Integer,
    VariableKind::SemiContinuous, VariableKind::SemiInteger,
};

struct Variable
{
    VariableKind kind;
    double lower;
    double upper;
};

Variable widest(VariableKind kind) noexcept;

// Variables are numbered from 0 in the order they are added. Each one held has a known kind, no
// NaN bound, no lower bound of infinity, no upper bound of minus infinity, and bounds within the
// widest of its kind; a lower bound above the upper one is kept. A refused call changes nothing.
class Model
{
  public:
    Index variable_count() const noexcept;
    const Variable &variable(Index index) const;
    std::span<const Variable> variables(Index first, Index count) const;

    void add_variables(std::span<const Variable> added);
    void set_variables(std::span<const Index> indices, std::span<const Variable> changed);

  private:
    std::vector<Variable> variables_;
};

}
