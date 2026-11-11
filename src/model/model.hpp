#pragma once

#include "biyos/model.h"
#include "core/index.hpp"

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

struct Row
{
    double lower;
    double upper;
};

struct Entry
{
    Index column;
    double value;
};

void check_starts(std::span<const Offset> starts);

// Variables and rows are numbered from 0 in the order they are added. Each variable held has a
// known kind, no NaN bound, no lower bound of infinity, no upper bound of minus infinity, and
// bounds within the widest of its kind; a lower bound above the upper one is kept. A row's bounds
// follow the rule of a continuous variable. Row r's entries lie in entries_ from ends_[r - 1], or
// 0 for row 0, to ends_[r], sorted by column, each column a variable appearing once with a finite
// coefficient, zeros kept. Empty vectors are an empty model, so a moved-from one stays valid. A
// refused call changes nothing.
class Model
{
  public:
    Index variable_count() const noexcept;
    const Variable &variable(Index index) const;
    std::span<const Variable> variables(Index first, Index count) const;

    void add_variables(std::span<const Variable> added);
    void set_variables(std::span<const Index> indices, std::span<const Variable> changed);

    Index row_count() const noexcept;
    const Row &row(Index index) const;
    std::span<const Row> rows(Index first, Index count) const;
    std::span<const Entry> entries(Index first, Index count) const;

    void check_room(std::size_t rows, Offset entries) const;
    void add_rows(std::span<const Row> added, std::span<const Offset> starts,
                  std::vector<Entry> entries);
    void set_rows(std::span<const Index> indices, std::span<const Row> changed);

  private:
    Offset begin_of(std::size_t row) const noexcept;
    void append(std::span<const Row> added, std::span<const Offset> starts,
                std::span<const Entry> entries);

    std::vector<Variable> variables_;
    std::vector<Row> rows_;
    std::vector<Offset> ends_;
    std::vector<Entry> entries_;
};

}
