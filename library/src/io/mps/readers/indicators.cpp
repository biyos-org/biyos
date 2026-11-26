#include "io/mps/readers/indicators.hpp"

#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "io/draft/draft.hpp"
#include "io/lines.hpp"
#include "io/records.hpp"
#include "io/report.hpp"
#include "model/model.hpp"

#include <string_view>

namespace biyos
{

namespace
{

Index indicated_row(const Lines &lines, const Draft &draft, std::string_view name)
{
    const Index index = find_in(draft.row_names, lines, "row", name);
    const DraftRow &row = draft.rows[to_size(index)];
    if (row.sense == RowSense::Free)
    {
        bad_file(lines, "N row " + quote(name) + " cannot hold an indicator");
    }
    if (row.range)
    {
        bad_file(lines, "row " + quote(name) + " has a range, so it cannot hold an indicator");
    }
    return index;
}

Index switch_column(const Lines &lines, Draft &draft, std::string_view name)
{
    const Index index = find_in(draft.column_names, lines, "column", name);
    DraftColumn &column = draft.columns[to_size(index)];
    const bool unit = is_zero(lower_of(column)) && is_zero(upper_of(column) - 1.0);
    if (kind_of(column) == VariableKind::Integer && unit)
    {
        column.binary = true;
        column.lower = 0.0;
        column.upper = 1.0;
    }
    if (kind_of(column) != VariableKind::Binary)
    {
        bad_file(lines, "column " + quote(name) + " is not binary");
    }
    return index;
}

int switch_value(const Lines &lines, std::string_view field)
{
    const Real value = value_of(lines, field);
    if (is_zero(value))
    {
        return 0;
    }
    if (is_zero(value - 1.0))
    {
        return 1;
    }
    bad_file(lines, "indicator value " + quote(field) + " is not 0 or 1");
}

}

void read_indicator(const Lines &lines, Draft &draft)
{
    const Fields fields = uncommented(lines.fields(), 2);
    if (fields.size() != 4 || fields[0] != "IF")
    {
        bad_file(lines, "an indicator line holds IF, a row, a column and a value");
    }
    draft.indicators.push_back(DraftIndicator{
        .row = indicated_row(lines, draft, fields[1]),
        .column = switch_column(lines, draft, fields[2]),
        .value = switch_value(lines, fields[3]),
    });
}

}
