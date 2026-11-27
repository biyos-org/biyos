#include "io/nl/source/binary.hpp"

#include "core/env/error.hpp"
#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "io/utils/records.hpp"
#include "io/utils/report.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <string>
#include <string_view>
#include <utility>

namespace biyos::nl
{

namespace
{

constexpr std::size_t kDoubleSize = 8;
constexpr std::size_t kIntSize = 4;
constexpr std::size_t kShortSize = 2;
constexpr unsigned char kDelete = 0x7f;

std::string shown(char key)
{
    const auto code = static_cast<unsigned char>(key);
    if (code > ' ' && code < kDelete)
    {
        return std::string(1, key);
    }
    return "\\x" + hex_of(code);
}

}

BinarySource::BinarySource(std::istream &input, std::string file, std::endian order, Offset bytes)
    : input_(input), file_(std::move(file)), order_(order), bytes_(bytes),
      at_(bytes - size_of(input)), start_(at_)
{
}

bool BinarySource::next()
{
    begin("a segment");
    char key = '\0';
    if (take(&key, 1) == 0)
    {
        return false;
    }
    key_ = key;
    return true;
}

void BinarySource::item(std::string_view part)
{
    begin(part);
    key_ = byte();
}

void BinarySource::line(std::string_view part)
{
    begin(part);
}

char BinarySource::key() const noexcept
{
    return key_;
}

std::string BinarySource::key_text() const
{
    return shown(key_);
}

void BinarySource::expect(std::size_t) const {}

Offset BinarySource::integer()
{
    const auto value = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits(kIntSize)));
    last_ = text_of(value);
    return value;
}

Real BinarySource::real()
{
    return std::bit_cast<Real>(bits(kDoubleSize));
}

std::string BinarySource::name()
{
    const Index n = count();
    if (n > bytes_ - at_)
    {
        cut_short();
    }
    std::string text(to_size(n), '\0');
    if (take(text.data(), text.size()) != text.size())
    {
        cut_short();
    }
    return text;
}

Offset BinarySource::code()
{
    const char code = byte();
    last_ = shown(code);
    return Offset{code} - '0';
}

bool BinarySource::is_constant(char key) const noexcept
{
    return key == 'n' || key == 's' || key == 'l';
}

Real BinarySource::constant(char leaf)
{
    if (leaf == 's')
    {
        return static_cast<Real>(small());
    }
    if (leaf == 'l')
    {
        return static_cast<Real>(integer());
    }
    return real();
}

std::string BinarySource::last_field() const
{
    return last_;
}

std::string BinarySource::where() const
{
    return "file '" + file_ + "' byte " + text_of(start_);
}

void BinarySource::inform(Stream stream, const std::string &text) const
{
    biyos::inform(stream, file_, "byte", to_size(start_), text);
}

std::size_t BinarySource::take(char *data, std::size_t n)
{
    input_.read(data, static_cast<std::streamsize>(n));
    if (input_.bad())
    {
        throw Error(ErrorKind::FileAccess, "file '" + file_ + "'", "cannot be read");
    }
    const auto taken = static_cast<std::size_t>(input_.gcount());
    at_ += static_cast<Offset>(taken);
    return taken;
}

std::uint64_t BinarySource::bits(std::size_t n)
{
    std::array<char, sizeof(std::uint64_t)> raw{};
    if (take(raw.data(), n) != n)
    {
        cut_short();
    }
    std::uint64_t value = 0;
    for (std::size_t k = 0; k < n; ++k)
    {
        const std::size_t at = order_ == std::endian::little ? n - 1 - k : k;
        value = value << 8U | static_cast<unsigned char>(raw[at]);
    }
    return value;
}

char BinarySource::byte()
{
    char value = '\0';
    if (take(&value, 1) != 1)
    {
        cut_short();
    }
    return value;
}

void BinarySource::cut_short() const
{
    bad("the file ends inside " + part_);
}

void BinarySource::begin(std::string_view part)
{
    part_ = part;
    start_ = at_;
}

Offset BinarySource::small()
{
    const auto value = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits(kShortSize)));
    last_ = text_of(value);
    return value;
}

}
