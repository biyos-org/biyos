#include "io/nl/tokens.hpp"

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>

namespace biyos::nl
{

namespace
{

bool comment(std::string_view field) noexcept
{
    return field.starts_with('#');
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

}
