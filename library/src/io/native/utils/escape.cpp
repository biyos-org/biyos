#include "io/native/utils/escape.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace biyos::native
{

namespace
{

constexpr std::string_view kDigits = "0123456789ABCDEF";
constexpr unsigned char kDelete = 0x7F;
constexpr std::size_t kBase = 16;

bool plain(unsigned char byte) noexcept
{
    return byte > ' ' && byte != '\\' && byte != kDelete;
}

std::optional<std::size_t> digit_of(char digit) noexcept
{
    const std::size_t at = kDigits.find(digit);
    if (at == std::string_view::npos)
    {
        return std::nullopt;
    }
    return at;
}

}

std::string escaped(std::string_view name)
{
    std::string field;
    field.reserve(name.size());
    for (const char letter : name)
    {
        const auto byte = static_cast<unsigned char>(letter);
        if (plain(byte))
        {
            field += letter;
            continue;
        }
        field += '\\';
        field += kDigits[byte / kBase];
        field += kDigits[byte % kBase];
    }
    return field;
}

std::optional<std::string> unescaped(std::string_view field)
{
    std::string name;
    name.reserve(field.size());
    for (std::size_t k = 0; k < field.size(); ++k)
    {
        if (field[k] != '\\')
        {
            name += field[k];
            continue;
        }
        if (k + 2 >= field.size())
        {
            return std::nullopt;
        }
        const std::optional<std::size_t> high = digit_of(field[k + 1]);
        const std::optional<std::size_t> low = digit_of(field[k + 2]);
        if (!high || !low)
        {
            return std::nullopt;
        }
        name += static_cast<char>(*high * kBase + *low);
        k += 2;
    }
    return name;
}

}
