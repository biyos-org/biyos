#include "io/nl/header.hpp"

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "io/nl/tokens.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::nl
{

namespace
{

constexpr std::size_t kRows = 9;
constexpr std::size_t kSizes = 0;
constexpr std::size_t kComplements = 1;
constexpr std::size_t kNonlinear = 3;
constexpr std::size_t kArcs = 4;
constexpr std::size_t kDiscrete = 5;
constexpr std::size_t kCommon = 8;
constexpr std::size_t kColumns = 5;
constexpr std::size_t kArith = 2;
constexpr Offset kLineBytes = 2;
constexpr Offset kRecordByte = 1;
constexpr Index kLittle = 1;
constexpr Index kBig = 2;
constexpr std::string_view kOtherFormats = "hzBGHZ";
constexpr std::array<std::size_t, kRows> kWidths{5, 2, 2, 3, 4, 5, 2, 2, 5};

using Rows = std::array<std::array<Index, kColumns>, kRows>;

struct Layout
{
    Index in_constraints;
    Index in_objectives;
    Index in_both;
    Index arcs;
    Index binaries;
    Index integers;
    Index both_integers;
    Index constraint_integers;
    Index objective_integers;
};

struct Block
{
    Index begin;
    Index end;
    VariableKind kind;
};

char read_first(Lines &lines)
{
    if (!advance(lines))
    {
        bad_file(lines, "is empty");
    }
    const char format = Entry(lines).key();
    if (kOtherFormats.find(format) != std::string_view::npos)
    {
        unsupported(lines, "NL format " + quote(std::string(1, format)));
    }
    if (format != 'g' && format != 'b')
    {
        bad_file(lines, "the file does not start with 'g' or 'b'");
    }
    return format;
}

Encoding encoding_of(const Lines &lines, char format, Index arith)
{
    if (format == 'g')
    {
        return Encoding::Text;
    }
    if (arith == kLittle)
    {
        return Encoding::Little;
    }
    if (arith == kBig)
    {
        return Encoding::Big;
    }
    unsupported(lines, "arithmetic kind " + text_of(arith));
}

Offset record_bytes(Encoding encoding) noexcept
{
    switch (encoding)
    {
    case Encoding::Text:
        return kLineBytes;
    case Encoding::Little:
    case Encoding::Big:
        return kRecordByte;
    }
    return kRecordByte;
}

std::array<Index, kColumns> read_row(Lines &lines, std::size_t width)
{
    advance_in(lines, "the header");
    const Fields fields = fields_of(lines);
    if (fields.size() < width)
    {
        bad_file(lines, "the header line holds fewer than " + text_of(width) + " values");
    }
    std::array<Index, kColumns> row{};
    for (std::size_t k = 0; k < fields.size(); ++k)
    {
        const Index count = count_of(lines, fields[k]);
        if (k < kColumns)
        {
            row[k] = count;
        }
    }
    return row;
}

Layout layout_of(const Rows &rows) noexcept
{
    return {
        .in_constraints = rows[kNonlinear][0],
        .in_objectives = rows[kNonlinear][1],
        .in_both = rows[kNonlinear][2],
        .arcs = rows[kArcs][0],
        .binaries = rows[kDiscrete][0],
        .integers = rows[kDiscrete][1],
        .both_integers = rows[kDiscrete][2],
        .constraint_integers = rows[kDiscrete][3],
        .objective_integers = rows[kDiscrete][4],
    };
}

Offset nonlinear_of(const Layout &layout) noexcept
{
    return (std::max)(layout.in_constraints, layout.in_objectives);
}

bool adds_up(const Layout &layout, Index variables) noexcept
{
    const Offset nonlinear = nonlinear_of(layout);
    const Offset only_objectives = nonlinear - layout.in_constraints;
    const Offset linear = Offset{layout.arcs} + layout.binaries + layout.integers;
    return layout.both_integers <= layout.in_both &&
           layout.in_both <= (std::min)(layout.in_constraints, layout.in_objectives) &&
           Offset{layout.in_both} + layout.constraint_integers <= layout.in_constraints &&
           layout.objective_integers <= only_objectives && nonlinear + linear <= variables;
}

std::vector<VariableKind> kinds_of(const Layout &layout, Index variables)
{
    const auto nonlinear = static_cast<Index>(nonlinear_of(layout));
    const Index linear_integers = variables - layout.integers;
    const std::array blocks{
        Block{layout.in_both - layout.both_integers, layout.in_both, VariableKind::Integer},
        Block{layout.in_constraints - layout.constraint_integers,
              layout.in_constraints,
              VariableKind::Integer},
        Block{nonlinear - layout.objective_integers, nonlinear, VariableKind::Integer},
        Block{linear_integers - layout.binaries, linear_integers, VariableKind::Binary},
        Block{linear_integers, variables, VariableKind::Integer},
    };
    std::vector<VariableKind> kinds(to_size(variables), VariableKind::Continuous);
    for (const Block &block : blocks)
    {
        std::fill(kinds.begin() + block.begin, kinds.begin() + block.end, block.kind);
    }
    return kinds;
}

Index defined_of(const Lines &lines, const Rows &rows)
{
    const std::array<Index, kColumns> &common = rows[kCommon];
    const Offset defined = std::accumulate(common.begin(), common.end(), Offset{0});
    if (defined > kMostIndex - rows[kSizes][0])
    {
        bad_file(lines, "the header counts more variables than a model holds");
    }
    return static_cast<Index>(defined);
}

void check_functions(const Lines &lines, const Rows &rows)
{
    if (Offset{rows[kSizes][1]} + rows[kSizes][2] > kMostIndex)
    {
        bad_file(lines, "the header counts more functions than a model holds");
    }
}

Index complements_of(const Lines &lines, const Rows &rows)
{
    const std::array<Index, kColumns> &counts = rows[kComplements];
    const Offset complements = Offset{counts[2]} + counts[3];
    if (complements > rows[kSizes][1])
    {
        bad_file(lines, "the header counts more complements than constraints");
    }
    return static_cast<Index>(complements);
}

void check_room(const Lines &lines, const Rows &rows, Index defined, Offset room)
{
    const std::array<Index, kColumns> &sizes = rows[kSizes];
    const Offset records = Offset{sizes[0]} + sizes[1] + sizes[2] + defined;
    if (records > room)
    {
        bad_file(lines, "the header counts more records than the file holds");
    }
}

}

Header read_header(Lines &lines, Offset bytes)
{
    const char format = read_first(lines);
    Rows rows{};
    for (std::size_t k = 0; k < kRows; ++k)
    {
        rows[k] = read_row(lines, kWidths[k]);
    }
    const Encoding encoding = encoding_of(lines, format, rows[kArcs][kArith]);
    const Layout layout = layout_of(rows);
    const Index variables = rows[kSizes][0];
    if (!adds_up(layout, variables))
    {
        bad_file(lines, "the header's variable counts do not add up");
    }
    check_functions(lines, rows);
    const Index defined = defined_of(lines, rows);
    const Index complements = complements_of(lines, rows);
    check_room(lines, rows, defined, bytes / record_bytes(encoding));
    return {
        .variables = variables,
        .constraints = rows[kSizes][1],
        .objectives = rows[kSizes][2],
        .defined = defined,
        .complements = complements,
        .kinds = kinds_of(layout, variables),
        .encoding = encoding,
    };
}

}
