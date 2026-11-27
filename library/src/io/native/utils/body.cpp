#include "io/native/utils/body.hpp"

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "io/native/utils/record.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/report.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace biyos::native
{

Body::Body(Lines &lines, std::string word, Offset count)
    : lines_(lines), word_(std::move(word)), left_(count)
{
}

bool Body::next()
{
    if (left_ == 0)
    {
        return false;
    }
    --left_;
    follow();
    return true;
}

void Body::follow()
{
    if (!next_line(lines_))
    {
        bad_file(lines_, "the file ends inside " + word_);
    }
}

Record Body::record() const noexcept
{
    return Record(lines_);
}

bool next_line(Lines &lines)
{
    while (lines.next())
    {
        if (!lines.fields().empty())
        {
            return true;
        }
    }
    return false;
}

void read_start(Lines &lines, std::string_view whole, std::string_view magic, int newest)
{
    const bool found = next_line(lines);
    if (lines.number() == 0)
    {
        bad_file(lines, "is empty");
    }
    if (!found || lines.fields().front() != magic)
    {
        bad_file(lines, std::string(whole) + " does not start with " + std::string(magic));
    }
    Record record(lines);
    record.word();
    const int version = record.integer();
    if (version > newest)
    {
        unsupported(lines, "version " + text_of(version) + " is newer than " + text_of(newest));
    }
    record.end();
    if (version < 1)
    {
        record.refuse("version " + text_of(version) + " is unknown");
    }
}

}
