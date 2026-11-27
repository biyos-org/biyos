#include "io/nl/source/source.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "io/nl/tokens.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"

#include <ios>
#include <istream>
#include <string>
#include <string_view>

namespace biyos::nl
{

Index Source::count()
{
    const Offset value = integer();
    if (value < 0 || value > kMostIndex)
    {
        bad(quote(token()) + " is not a count");
    }
    return static_cast<Index>(value);
}

Index Source::index(Index size, std::string_view noun)
{
    const Index index = count();
    if (index >= size)
    {
        bad(std::string(noun) + " " + text_of(index) + " is out of range");
    }
    return index;
}

void Source::bad(const std::string &message) const
{
    bad_file(where(), message);
}

void Source::refuse(const std::string &message) const
{
    unsupported(where(), message);
}

void reject(const Source &source, const Error &error)
{
    biyos::reject(source.where(), error);
}

Offset size_of(std::istream &input)
{
    const std::streampos start = input.tellg();
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg() - start;
    input.seekg(start);
    return size;
}

}
