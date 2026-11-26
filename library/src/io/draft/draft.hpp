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
    std::optional<Real> rhs;
    std::optional<Real> range;
};

struct DraftColumn
{
    bool marked = false;
    bool integer = false;
    bool semi = false;
    bool binary = false;
    std::optional<Real> lower;
    std::optional<Real> upper;
    std::size_t upper_line = 0;
};

struct DraftEntry
{
    Index row;
    Index column;
    Real value;
};

struct DraftSet
{
    SpecialOrderedSetKind kind;
    std::optional<std::string> name;
    std::vector<Member> members;
};

struct DraftIndicator
{
    Index row;
    Index column;
    int value;
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
    std::vector<DraftSet> sets;
    std::vector<DraftIndicator> indicators;
};

bool negative_upper(const DraftColumn &column) noexcept;
VariableKind kind_of(const DraftColumn &column) noexcept;
Real lower_of(const DraftColumn &column) noexcept;
Real upper_of(const DraftColumn &column) noexcept;
Constraint constraint_of(Index function, const DraftRow &row) noexcept;
Model build(const Draft &draft);
[[noreturn]] void unwritable(const std::string &object, const std::string &part);

}
