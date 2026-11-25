#pragma once

#include "core/numbers.hpp"
#include "io/directory.hpp"
#include "model/model.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace biyos
{

enum class RowSense
{
    Free,
    Less,
    Greater,
    Equal,
};

struct DraftRow
{
    RowSense sense;
    std::optional<double> rhs;
    std::optional<double> range;
};

struct DraftColumn
{
    bool marked = false;
    bool integer = false;
    bool semi = false;
    bool binary = false;
    std::optional<double> lower;
    std::optional<double> upper;
    std::size_t upper_line = 0;
};

struct DraftEntry
{
    Index row;
    Index column;
    double value;
};

struct Draft
{
    std::optional<std::string> name;
    ObjectiveSense sense = ObjectiveSense::Minimize;
    std::vector<DraftRow> rows;
    Directory row_names;
    std::vector<DraftColumn> columns;
    Directory column_names;
    std::vector<DraftEntry> entries;
};

bool negative_upper(const DraftColumn &column) noexcept;
VariableKind kind_of(const DraftColumn &column) noexcept;
double lower_of(const DraftColumn &column) noexcept;
double upper_of(const DraftColumn &column) noexcept;
Constraint constraint_of(Index function, const DraftRow &row) noexcept;
Model build(const Draft &draft);

}
