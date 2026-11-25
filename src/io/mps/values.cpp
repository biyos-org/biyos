#include "io/mps/values.hpp"

#include "core/env.hpp"
#include "core/numbers.hpp"
#include "io/draft/draft.hpp"
#include "io/lines.hpp"
#include "io/mps/vectors.hpp"
#include "io/records.hpp"
#include "io/report.hpp"

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
    double value;
};

template <typename Set>
void read_pairs(Env &env,
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
    if (!vectors.keeps(env, lines, fields[0]))
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

void set_range(Env &env, const Lines &lines, Draft &draft, const Value &value)
{
    DraftRow &row = draft.rows[to_size(value.row)];
    if (row.sense == RowSense::Free)
    {
        warn(env, lines, "the range of N row " + quote(value.name) + " is ignored");
        return;
    }
    if (row.range)
    {
        bad_file(lines, "row " + quote(value.name) + " has a second range");
    }
    row.range = value.value;
}

}

void read_rhs(Env &env, const Lines &lines, Vectors &vectors, Draft &draft)
{
    const auto set = [&](const Value &value) { set_rhs(lines, draft, value); };
    read_pairs(env, lines, vectors, draft, "RHS", set);
}

void read_range(Env &env, const Lines &lines, Vectors &vectors, Draft &draft)
{
    const auto set = [&](const Value &value) { set_range(env, lines, draft, value); };
    read_pairs(env, lines, vectors, draft, "RANGES", set);
}

}
