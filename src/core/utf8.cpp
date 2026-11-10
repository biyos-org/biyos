#include "core/utf8.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace biyos::utf8
{

namespace
{

struct Range
{
    unsigned char low;
    unsigned char high;

    bool holds(unsigned char byte) const noexcept { return low <= byte && byte <= high; }
};

struct Row
{
    Range first;
    std::size_t size;
    Range second;
};

constexpr Range kTrail{.low = 0x80, .high = 0xbf};

constexpr std::array kRows{
    Row{.first = {0x00, 0x7f}, .size = 1, .second = {}},
    Row{.first = {0xc2, 0xdf}, .size = 2, .second = kTrail},
    Row{.first = {0xe0, 0xe0}, .size = 3, .second = {0xa0, 0xbf}},
    Row{.first = {0xe1, 0xec}, .size = 3, .second = kTrail},
    Row{.first = {0xed, 0xed}, .size = 3, .second = {0x80, 0x9f}},
    Row{.first = {0xee, 0xef}, .size = 3, .second = kTrail},
    Row{.first = {0xf0, 0xf0}, .size = 4, .second = {0x90, 0xbf}},
    Row{.first = {0xf1, 0xf3}, .size = 4, .second = kTrail},
    Row{.first = {0xf4, 0xf4}, .size = 4, .second = {0x80, 0x8f}},
};

unsigned char byte_at(std::string_view text, std::size_t at) noexcept
{
    return static_cast<unsigned char>(text[at]);
}

const Row *row_of(unsigned char first) noexcept
{
    for (const Row &row : kRows)
    {
        if (row.first.holds(first))
        {
            return &row;
        }
    }
    return nullptr;
}

bool continues(const Row &row, std::size_t at, unsigned char byte) noexcept
{
    const Range &range = at == 1 ? row.second : kTrail;
    return range.holds(byte);
}

}

Sequence first_sequence(std::string_view text) noexcept
{
    const Row *row = row_of(byte_at(text, 0));
    if (row == nullptr)
    {
        return Sequence{.size = 1, .valid = false};
    }
    std::size_t size = 1;
    while (size < row->size && size < text.size() && continues(*row, size, byte_at(text, size)))
    {
        ++size;
    }
    return Sequence{.size = size, .valid = size == row->size};
}

}
