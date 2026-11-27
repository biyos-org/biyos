#include "io/nl/source/text.hpp"

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "io/nl/tokens.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace biyos::nl
{

TextSource::TextSource(Lines &lines) : lines_(lines), entry_(lines) {}

bool TextSource::next()
{
    if (!advance(lines_))
    {
        return false;
    }
    start(true);
    return true;
}

void TextSource::item(std::string_view part)
{
    advance_in(lines_, part);
    start(true);
}

void TextSource::line(std::string_view part)
{
    advance_in(lines_, part);
    start(false);
}

char TextSource::key() const noexcept
{
    return entry_.key();
}

std::string TextSource::head() const
{
    return std::string(fields_of(lines_).front());
}

void TextSource::expect(std::size_t n) const
{
    check_size(lines_, size(), n);
}

Offset TextSource::integer()
{
    return integer_of(lines_, take());
}

Real TextSource::real()
{
    return value_of(lines_, take());
}

std::string TextSource::name()
{
    return std::string(take());
}

Offset TextSource::code()
{
    return integer();
}

bool TextSource::holds(char leaf) const noexcept
{
    return leaf == 'n';
}

Real TextSource::constant(char)
{
    return real();
}

std::string TextSource::token() const
{
    return std::string(token_);
}

std::string TextSource::where() const
{
    return lines_.where();
}

void TextSource::inform(Stream stream, const std::string &text) const
{
    biyos::inform(stream, lines_, text);
}

void TextSource::start(bool keyed)
{
    keyed_ = keyed;
    if (keyed)
    {
        entry_ = Entry(lines_);
    }
    else
    {
        fields_ = fields_of(lines_);
    }
    taken_ = 0;
}

std::size_t TextSource::size() const noexcept
{
    return keyed_ ? entry_.size() : fields_.size();
}

std::string_view TextSource::take()
{
    if (taken_ == size())
    {
        check_size(lines_, size(), taken_ + 1);
    }
    token_ = keyed_ ? entry_[taken_] : fields_[taken_];
    ++taken_;
    return token_;
}

}
