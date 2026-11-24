#include "io/rows.hpp"

#include "io/draft/draft.hpp"
#include "io/lines.hpp"
#include "io/records.hpp"
#include "io/report.hpp"
#include "model/model.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

namespace biyos
{

namespace
{

struct Letter
{
    std::string_view code;
    RowSense sense;
};

constexpr std::array kLetters{
    Letter{"N", RowSense::Free},
    Letter{"L", RowSense::Less},
    Letter{"G", RowSense::Greater},
    Letter{"E", RowSense::Equal},
};

std::optional<RowSense> row_sense(std::string_view code) noexcept
{
    const auto found = std::ranges::find(kLetters, code, &Letter::code);
    if (found == kLetters.end())
    {
        return std::nullopt;
    }
    return found->sense;
}

}

void read_row(const Lines &lines, Draft &draft)
{
    const Fields fields = uncommented(lines.fields(), 2);
    if (fields.size() > 2 && fields[0] == "N")
    {
        unsupported(lines, "N row " + quote(fields[1]) + " carries objective fields");
    }
    if (fields.size() != 2)
    {
        bad_file(lines, "a row needs a sense and a name");
    }
    const std::optional<RowSense> sense = row_sense(fields[0]);
    if (!sense)
    {
        bad_file(lines, "unknown row sense " + quote(fields[0]));
    }
    check_name(lines, fields[1]);
    if (!draft.row_names.add(fields[1]))
    {
        bad_file(lines, "row " + quote(fields[1]) + " is declared twice");
    }
    draft.rows.push_back(DraftRow{.sense = *sense, .rhs = std::nullopt, .range = std::nullopt});
}

ObjectiveSense sense_of(const Lines &lines, std::string_view word)
{
    if (word == "MIN")
    {
        return ObjectiveSense::Minimize;
    }
    if (word == "MAX")
    {
        return ObjectiveSense::Maximize;
    }
    bad_file(lines, "unknown objective sense " + quote(word));
}

std::string_view letter_of(RowSense sense) noexcept
{
    return std::ranges::find(kLetters, sense, &Letter::sense)->code;
}

}
