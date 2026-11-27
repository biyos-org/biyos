#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace biyos::test::nl
{

inline constexpr std::size_t kHeaderLines = 10;
inline constexpr std::size_t kArithLine = 5;
inline constexpr unsigned kByteBits = 8;
inline constexpr std::uint64_t kByteMask = 0xff;

class Bytes
{
  public:
    explicit Bytes(std::endian order) : order_(order) {}

    Bytes &segment(char key)
    {
        starts_.push_back(text_.size());
        keys_.push_back(key);
        return byte(key);
    }

    Bytes &byte(char value)
    {
        text_.push_back(value);
        return *this;
    }

    Bytes &integer(std::int32_t value)
    {
        return put(std::bit_cast<std::uint32_t>(value), sizeof value);
    }

    Bytes &small(std::int16_t value)
    {
        return put(std::bit_cast<std::uint16_t>(value), sizeof value);
    }

    Bytes &real(double value) { return put(std::bit_cast<std::uint64_t>(value), sizeof value); }

    Bytes &name(std::string_view value)
    {
        integer(static_cast<std::int32_t>(value.size()));
        text_.append(value);
        return *this;
    }

    const std::string &text() const noexcept { return text_; }

    const std::vector<std::size_t> &starts() const noexcept { return starts_; }

    const std::string &keys() const noexcept { return keys_; }

  private:
    Bytes &put(std::uint64_t bits, std::size_t n)
    {
        for (std::size_t k = 0; k < n; ++k)
        {
            const std::size_t shift = order_ == std::endian::little ? k : n - 1 - k;
            text_.push_back(static_cast<char>(bits >> (kByteBits * shift) & kByteMask));
        }
        return *this;
    }

    std::endian order_;
    std::string text_;
    std::vector<std::size_t> starts_;
    std::string keys_;
};

inline std::string arith_of(std::endian order)
{
    return order == std::endian::little ? "1" : "2";
}

inline std::string binary_header(std::string_view text, std::string_view arith)
{
    std::string header;
    for (std::size_t line = 0; line < kHeaderLines; ++line)
    {
        const std::size_t end = text.find('\n') + 1;
        std::string row(text.substr(0, end));
        text.remove_prefix(end);
        if (line == 0)
        {
            row.front() = 'b';
        }
        if (line == kArithLine)
        {
            row = " 0 0 " + std::string(arith) + " 1\n";
        }
        header += row;
    }
    return header;
}

}
