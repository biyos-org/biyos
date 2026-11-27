#include "io/native/utils/record.hpp"

#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "core/text/text.hpp"
#include "io/native/parts.hpp"
#include "io/native/utils/escape.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace biyos::native
{

namespace
{

template <typename T> std::optional<T> whole_of(std::string_view field) noexcept
{
    T value = 0;
    const char *last = field.data() + field.size();
    const std::from_chars_result result = std::from_chars(field.data(), last, value);
    if (result.ec != std::errc() || result.ptr != last)
    {
        return std::nullopt;
    }
    return value;
}

}

Record::Record(const Lines &lines) noexcept : lines_(lines) {}

std::string_view Record::word()
{
    return take();
}

Index Record::index()
{
    const std::string_view field = take();
    const std::optional<Index> value = whole_of<Index>(field);
    if (!value)
    {
        refuse(quote(field) + " is not an index");
    }
    return *value;
}

int Record::integer()
{
    const std::string_view field = take();
    const std::optional<int> value = whole_of<int>(field);
    if (!value)
    {
        refuse(quote(field) + " is not an integer");
    }
    return *value;
}

Offset Record::count()
{
    const std::string_view field = take();
    const std::optional<Offset> value = whole_of<Offset>(field);
    if (!value || *value < 0)
    {
        refuse(quote(field) + " is not a count");
    }
    return *value;
}

Offset Record::entries(std::size_t width)
{
    const Offset value = count();
    const std::size_t left = lines_.fields().size() - next_;
    if (to_size(value) > left / width)
    {
        refuse("the line holds fewer than " + text_of(value) + " entries");
    }
    return value;
}

Real Record::real()
{
    const std::string_view field = take();
    if (field == "inf")
    {
        return kInfinity;
    }
    if (field == "-inf")
    {
        return -kInfinity;
    }
    return value_of(lines_, field);
}

std::optional<Real> Record::tolerance()
{
    if (next_ < lines_.fields().size() && lines_.fields()[next_] == kUnset)
    {
        ++next_;
        return std::nullopt;
    }
    return real();
}

std::string Record::name()
{
    const std::string_view field = take();
    std::optional<std::string> name = unescaped(field);
    if (!name)
    {
        refuse(quote(field) + " holds a bad escape");
    }
    return std::move(*name);
}

void Record::end() const
{
    if (next_ < lines_.fields().size())
    {
        refuse("the line holds too many fields");
    }
}

void Record::refuse(const std::string &message) const
{
    bad_file(lines_, message);
}

std::string_view Record::take()
{
    if (next_ == lines_.fields().size())
    {
        refuse("the line ends early");
    }
    return lines_.fields()[next_++];
}

}
