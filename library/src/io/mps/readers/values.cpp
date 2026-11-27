#include "io/mps/readers/values.hpp"

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "io/draft/draft.hpp"
#include "io/mps/readers/vectors.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

struct Value
{
    Index row;
    std::string_view name;
    Real value;
};

template <typename Set>
void read_pairs(Stream stream,
                const Lines &lines,
                Vectors &vectors,
                const Draft &draft,
                std::string_view section,
                Set set)
{
    const Fields fields = uncommented(lines.fields(), 1);
    if (fields.size() != 3 && fields.size() != 5)
    {
        bad_file(lines,
                 "an " + std::string(section) +
                     " line needs a vector and one or two pairs of row and value");
    }
    if (!vectors.keeps(stream, lines, fields[0]))
    {
        return;
    }
    for (std::size_t at = 1; at < fields.size(); at += 2)
    {
        const Index row = find_in(draft.row_names, lines, "row", fields[at]);
        set(Value{.row = row, .name = fields[at], .value = value_of(lines, fields[at + 1])});
    }
}

void set_rhs(const Lines &lines, Draft &draft, const Value &value)
{
    DraftRow &row = draft.rows[to_size(value.row)];
    if (row.rhs)
    {
        bad_file(lines, "row " + quote(value.name) + " has a second right-hand side");
    }
    row.rhs = value.value;
}

void set_range(Stream stream, const Lines &lines, Draft &draft, const Value &value)
{
    DraftRow &row = draft.rows[to_size(value.row)];
    if (row.sense == RowSense::Free)
    {
        warn(stream, lines, "the range of N row " + quote(value.name) + " is ignored");
        return;
    }
    if (row.range)
    {
        bad_file(lines, "row " + quote(value.name) + " has a second range");
    }
    row.range = value.value;
}

}

void read_rhs(Stream stream, const Lines &lines, Vectors &vectors, Draft &draft)
{
    const auto set = [&](const Value &value) { set_rhs(lines, draft, value); };
    read_pairs(stream, lines, vectors, draft, "RHS", set);
}

void read_range(Stream stream, const Lines &lines, Vectors &vectors, Draft &draft)
{
    const auto set = [&](const Value &value) { set_range(stream, lines, draft, value); };
    read_pairs(stream, lines, vectors, draft, "RANGES", set);
}

}
