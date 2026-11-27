#include "io/nl/source/source.hpp"

#include "core/env/error.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "io/nl/header.hpp"
#include "io/nl/source/binary.hpp"
#include "io/nl/source/text.hpp"
#include "io/nl/tokens.hpp"
#include "io/utils/lines.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"

#include <bit>
#include <ios>
#include <istream>
#include <memory>
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

std::unique_ptr<Source> source_of(Lines &lines,
                                  std::istream &input,
                                  Encoding encoding,
                                  Offset bytes)
{
    switch (encoding)
    {
    case Encoding::Text:
        return std::make_unique<TextSource>(lines);
    case Encoding::Little:
        return std::make_unique<BinarySource>(input, lines.file(), std::endian::little, bytes);
    case Encoding::Big:
        return std::make_unique<BinarySource>(input, lines.file(), std::endian::big, bytes);
    }
    throw Error(ErrorKind::Internal, "io", "the NL encoding is unknown");
}

}
