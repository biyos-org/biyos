#include "io/mps/columns.hpp"

#include "core/numbers.hpp"
#include "io/draft/draft.hpp"
#include "io/lines.hpp"
#include "io/records.hpp"
#include "io/report.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace biyos
{

void Columns::read(const Lines &lines, Draft &draft)
{
    const Fields fields = uncommented(lines.fields(), 1);
    if (fields.size() >= 3 && fields[1] == "'MARKER'")
    {
        mark(lines, fields[2]);
        return;
    }
    if (fields.size() % 2 == 0 || fields.size() > 5)
    {
        bad_file(lines, "a column line needs a name and up to two pairs of row and value");
    }
    const Index column = enter(lines, draft, fields[0]);
    for (std::size_t at = 1; at < fields.size(); at += 2)
    {
        add(lines, draft, column, fields.subspan(at, 2));
    }
}

void Columns::close(const Lines &lines) const
{
    if (marked_)
    {
        bad_file(lines, "an INTORG marker has no INTEND");
    }
}

void Columns::mark(const Lines &lines, std::string_view keyword)
{
    const bool start = keyword == "'INTORG'";
    if (!start && keyword != "'INTEND'")
    {
        bad_file(lines, "unknown marker " + std::string(keyword));
    }
    if (start == marked_)
    {
        const std::string place = marked_ ? " inside" : " outside";
        bad_file(lines, std::string(keyword) + place + " an integer block");
    }
    marked_ = start;
}

Index Columns::enter(const Lines &lines, Draft &draft, std::string_view name)
{
    if (current_ && draft.column_names.names()[to_size(*current_)] == name)
    {
        if (draft.columns[to_size(*current_)].marked != marked_)
        {
            bad_file(lines, "column " + quote(name) + " crosses a marker");
        }
        return *current_;
    }
    check_name(lines, name);
    if (!draft.column_names.add(name))
    {
        bad_file(lines, "column " + quote(name) + " is not contiguous");
    }
    if (last_.empty())
    {
        last_.assign(draft.rows.size(), -1);
    }
    DraftColumn added;
    added.marked = marked_;
    draft.columns.push_back(added);
    current_ = draft.column_names.size() - 1;
    return *current_;
}

void Columns::add(const Lines &lines, Draft &draft, Index column, Fields pair)
{
    const Index row = find_in(draft.row_names, lines, "row", pair[0]);
    const double value = value_of(lines, pair[1]);
    Index &last = last_[to_size(row)];
    if (last == column)
    {
        const std::string_view name = draft.column_names.names()[to_size(column)];
        bad_file(lines, "column " + quote(name) + " has row " + quote(pair[0]) + " twice");
    }
    last = column;
    draft.entries.push_back(DraftEntry{.row = row, .column = column, .value = value});
}

}
