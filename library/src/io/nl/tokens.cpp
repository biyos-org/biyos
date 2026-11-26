#include "io/nl/tokens.hpp"

#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "core/text/text.hpp"
#include "io/lines.hpp"
#include "io/records.hpp"
#include "io/report.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace biyos::nl
{

namespace
{

constexpr Offset kComplements = 5;

bool comment(std::string_view field) noexcept
{
    return field.starts_with('#');
}

Bounds coded_bounds(const Lines &lines, Fields fields, Offset code)
{
    switch (code)
    {
    case 0:
        check_size(lines, fields.size(), 3);
        return {.lower = value_of(lines, fields[1]), .upper = value_of(lines, fields[2])};
    case 1:
        check_size(lines, fields.size(), 2);
        return {.lower = -kInfinity, .upper = value_of(lines, fields[1])};
    case 2:
        check_size(lines, fields.size(), 2);
        return {.lower = value_of(lines, fields[1]), .upper = kInfinity};
    case 3:
        check_size(lines, fields.size(), 1);
        return {.lower = -kInfinity, .upper = kInfinity};
    case 4:
        check_size(lines, fields.size(), 2);
        return {.lower = value_of(lines, fields[1]), .upper = value_of(lines, fields[1])};
    default:
        break;
    }
    bad_file(lines, "bound code " + quote(fields[0]) + " is unknown");
}

}

Entry::Entry(const Lines &lines) noexcept : fields_(fields_of(lines))
{
    if (!fields_.empty())
    {
        glued_ = fields_.front().substr(1);
    }
}

char Entry::key() const noexcept
{
    return fields_.empty() ? '\0' : fields_.front().front();
}

std::size_t Entry::size() const noexcept
{
    if (fields_.empty())
    {
        return 0;
    }
    return fields_.size() - (glued_.empty() ? 1 : 0);
}

std::string_view Entry::operator[](std::size_t k) const noexcept
{
    if (glued_.empty())
    {
        return fields_[k + 1];
    }
    return k == 0 ? glued_ : fields_[k];
}

Fields fields_of(const Lines &lines) noexcept
{
    const Fields fields = lines.fields();
    const auto end = std::ranges::find_if(fields, comment);
    return fields.first(static_cast<std::size_t>(end - fields.begin()));
}

bool advance(Lines &lines)
{
    while (lines.next())
    {
        if (!fields_of(lines).empty())
        {
            return true;
        }
    }
    return false;
}

void advance_in(Lines &lines, std::string_view part)
{
    if (!advance(lines))
    {
        bad_file(lines, "the file ends inside " + std::string(part));
    }
}

void check_size(const Lines &lines, std::size_t held, std::size_t wanted)
{
    if (held != wanted)
    {
        bad_file(lines, "the line holds " + text_of(held) + " values, not " + text_of(wanted));
    }
}

Offset integer_of(const Lines &lines, std::string_view field)
{
    Offset value = 0;
    const char *last = field.data() + field.size();
    const std::from_chars_result result = std::from_chars(field.data(), last, value);
    if (result.ec != std::errc() || result.ptr != last)
    {
        bad_file(lines, quote(field) + " is not an integer");
    }
    return value;
}

Index count_of(const Lines &lines, std::string_view field)
{
    const Offset value = integer_of(lines, field);
    if (value < 0 || value > kMostIndex)
    {
        bad_file(lines, quote(field) + " is not a count");
    }
    return static_cast<Index>(value);
}

Index index_in(const Lines &lines, std::string_view field, Index size, std::string_view noun)
{
    const Index index = count_of(lines, field);
    if (index >= size)
    {
        bad_file(lines, std::string(noun) + " " + text_of(index) + " is out of range");
    }
    return index;
}

std::optional<Bounds> bounds_of(const Lines &lines)
{
    const Fields fields = fields_of(lines);
    const Offset code = integer_of(lines, fields.front());
    if (code == kComplements)
    {
        return std::nullopt;
    }
    return coded_bounds(lines, fields, code);
}

}
