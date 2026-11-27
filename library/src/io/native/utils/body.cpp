#include "io/native/utils/body.hpp"

#include "core/numbers.hpp"
#include "io/lines.hpp"
#include "io/native/utils/record.hpp"
#include "io/report.hpp"

#include <string>
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

}
