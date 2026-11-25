#include "io/mps/print.hpp"

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "io/draft/draft.hpp"
#include "io/identical.hpp"
#include "io/mps/rows.hpp"
#include "io/mps/sections.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <ostream>
#include <span>
#include <string_view>

namespace biyos
{

namespace
{

void header(std::ostream &output, Section section)
{
    output << word_of(section) << '\n';
}

void print_head(const Draft &draft, std::ostream &output)
{
    output << word_of(Section::Name);
    if (draft.name)
    {
        output << ' ' << *draft.name;
    }
    output << '\n';
    if (draft.sense == ObjectiveSense::Maximize)
    {
        output << word_of(Section::Sense) << " MAX\n";
    }
}

void print_rows(const Draft &draft, std::ostream &output)
{
    header(output, Section::Rows);
    const std::span<const std::string_view> names = draft.row_names.names();
    for (std::size_t k = 0; k < draft.rows.size(); ++k)
    {
        output << ' ' << letter_of(draft.rows[k].sense) << "  " << names[k] << '\n';
    }
}

void print_marker(std::ostream &output, std::string_view keyword)
{
    output << "    MARKER  'MARKER'  " << keyword << '\n';
}

auto in_column(std::size_t column) noexcept
{
    return [column](const DraftEntry &entry) { return to_size(entry.column) == column; };
}

void print_columns(const Draft &draft, std::ostream &output)
{
    header(output, Section::Columns);
    const std::span<const std::string_view> rows = draft.row_names.names();
    const std::span<const std::string_view> columns = draft.column_names.names();
    auto entry = draft.entries.begin();
    bool marked = false;
    for (std::size_t k = 0; k < draft.columns.size(); ++k)
    {
        if (draft.columns[k].marked != marked)
        {
            marked = draft.columns[k].marked;
            print_marker(output, marked ? "'INTORG'" : "'INTEND'");
        }
        const auto end = std::find_if_not(entry, draft.entries.end(), in_column(k));
        if (entry == end)
        {
            output << "    " << columns[k] << '\n';
        }
        for (; entry != end; ++entry)
        {
            const std::string_view row = rows[to_size(entry->row)];
            output << "    " << columns[k] << "  " << row << "  " << text_of(entry->value) << '\n';
        }
    }
    if (marked)
    {
        print_marker(output, "'INTEND'");
    }
}

void print_values(const Draft &draft,
                  std::ostream &output,
                  std::string_view vector,
                  std::optional<Real> DraftRow::*value)
{
    const std::span<const std::string_view> names = draft.row_names.names();
    for (std::size_t k = 0; k < draft.rows.size(); ++k)
    {
        if (const std::optional<Real> &stated = draft.rows[k].*value)
        {
            output << "    " << vector << "  " << names[k] << "  " << text_of(*stated) << '\n';
        }
    }
}

bool any_range(const Draft &draft)
{
    return std::ranges::any_of(draft.rows,
                               [](const DraftRow &row) { return row.range.has_value(); });
}

void print_bound(std::ostream &output,
                 std::string_view code,
                 std::string_view column,
                 std::optional<Real> value)
{
    output << ' ' << code << " BND  " << column;
    if (value)
    {
        output << "  " << text_of(*value);
    }
    output << '\n';
}

void print_lower(std::ostream &output, std::string_view column, Real lower)
{
    if (std::isinf(lower))
    {
        print_bound(output, "MI", column, std::nullopt);
        return;
    }
    print_bound(output, "LO", column, lower);
}

void print_upper(std::ostream &output, std::string_view column, Real upper)
{
    if (std::isinf(upper))
    {
        print_bound(output, "PL", column, std::nullopt);
        return;
    }
    print_bound(output, "UP", column, upper);
}

void print_column(std::ostream &output, std::string_view name, const DraftColumn &column)
{
    if (column.binary)
    {
        print_bound(output, "BV", name, std::nullopt);
        return;
    }
    if (!column.semi && column.lower && column.upper && identical(*column.lower, *column.upper))
    {
        print_bound(output, "FX", name, *column.lower);
        return;
    }
    if (column.lower)
    {
        print_lower(output, name, *column.lower);
    }
    if (column.upper && column.semi)
    {
        print_bound(output, column.integer ? "SI" : "SC", name, *column.upper);
        return;
    }
    if (column.upper)
    {
        print_upper(output, name, *column.upper);
    }
}

bool bounded(const DraftColumn &column) noexcept
{
    return column.binary || column.lower || column.upper;
}

void print_bounds(const Draft &draft, std::ostream &output)
{
    if (std::ranges::none_of(draft.columns, bounded))
    {
        return;
    }
    header(output, Section::Bounds);
    const std::span<const std::string_view> names = draft.column_names.names();
    for (std::size_t k = 0; k < draft.columns.size(); ++k)
    {
        print_column(output, names[k], draft.columns[k]);
    }
}

}

void print_mps(const Draft &draft, std::ostream &output)
{
    print_head(draft, output);
    print_rows(draft, output);
    print_columns(draft, output);
    header(output, Section::Rhs);
    print_values(draft, output, "RHS", &DraftRow::rhs);
    if (any_range(draft))
    {
        header(output, Section::Ranges);
        print_values(draft, output, "RNG", &DraftRow::range);
    }
    print_bounds(draft, output);
    header(output, Section::End);
}

}
