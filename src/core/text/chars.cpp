#include "core/text/chars.hpp"

#include "core/text/bignum.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <system_error>

namespace biyos::text
{

namespace
{

constexpr std::size_t kKept = 800;
constexpr std::size_t kWordDigits = 19;
constexpr std::int64_t kHighest = 309;
constexpr std::int64_t kLowest = -323;
constexpr std::int64_t kExponentCap = 1000000000000000;
constexpr std::int64_t kFastTens = 22;
constexpr std::uint64_t kExact = std::uint64_t{1} << 53;
constexpr std::uint64_t kHidden = std::uint64_t{1} << 52;
constexpr std::uint64_t kRounded = std::uint64_t{1} << 54;
constexpr std::int64_t kLeastShift = -1075;
constexpr std::int64_t kBias = 1075;
constexpr std::int64_t kInfinite = 2047;

static_assert((static_cast<std::int64_t>(kKept) - kLowest) * 10 / 3 + 64 <
              static_cast<std::int64_t>(Bignum::kBits));

struct Decimal
{
    std::array<std::uint8_t, kKept> digits{};
    std::size_t count = 0;
    std::int64_t point = 0;
    bool dropped = false;
};

struct Division
{
    std::uint64_t quotient;
    bool inexact;
};

constexpr std::array<double, kFastTens + 1> tens() noexcept
{
    std::array<double, kFastTens + 1> table{};
    table[0] = 1.0;
    for (std::size_t k = 1; k < table.size(); ++k)
    {
        table[k] = table[k - 1] * 10.0;
    }
    return table;
}

constexpr std::array<double, kFastTens + 1> kTens = tens();

bool is_digit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

void keep(Decimal &decimal, char c) noexcept
{
    if (decimal.count < kKept)
    {
        decimal.digits[decimal.count++] = static_cast<std::uint8_t>(c - '0');
    }
    else if (c != '0')
    {
        decimal.dropped = true;
    }
}

const char *integer_part(const char *p, const char *last, Decimal &decimal) noexcept
{
    for (; p != last && is_digit(*p); ++p)
    {
        if (decimal.count > 0 || *p != '0')
        {
            keep(decimal, *p);
            ++decimal.point;
        }
    }
    return p;
}

const char *fraction_part(const char *p, const char *last, Decimal &decimal) noexcept
{
    for (; p != last && is_digit(*p); ++p)
    {
        if (decimal.count > 0 || *p != '0')
        {
            keep(decimal, *p);
        }
        else
        {
            --decimal.point;
        }
    }
    return p;
}

const char *mantissa(const char *first, const char *last, Decimal &decimal) noexcept
{
    const char *end = integer_part(first, last, decimal);
    if (end == last || *end != '.')
    {
        return end;
    }
    const char *fraction = end + 1;
    const char *after = fraction_part(fraction, last, decimal);
    return end == first && after == fraction ? first : after;
}

const char *exponent(const char *p, const char *last, Decimal &decimal) noexcept
{
    if (p == last || (*p != 'e' && *p != 'E'))
    {
        return p;
    }
    const char *q = p + 1;
    const bool negative = q != last && *q == '-';
    if (q != last && (*q == '-' || *q == '+'))
    {
        ++q;
    }
    if (q == last || !is_digit(*q))
    {
        return p;
    }
    std::int64_t written = 0;
    for (; q != last && is_digit(*q); ++q)
    {
        written = (std::min)(written * 10 + (*q - '0'), kExponentCap);
    }
    decimal.point += negative ? -written : written;
    return q;
}

void trim(Decimal &decimal) noexcept
{
    while (decimal.count > 0 && decimal.digits[decimal.count - 1] == 0)
    {
        --decimal.count;
    }
}

std::int64_t tens_of(const Decimal &decimal) noexcept
{
    return decimal.point - static_cast<std::int64_t>(decimal.count);
}

std::optional<double> fast(const Decimal &decimal) noexcept
{
    const std::int64_t power = tens_of(decimal);
    if (decimal.dropped || decimal.count > kWordDigits || power < -kFastTens || power > kFastTens)
    {
        return std::nullopt;
    }
    std::uint64_t significand = 0;
    for (std::size_t k = 0; k < decimal.count; ++k)
    {
        significand = significand * 10 + decimal.digits[k];
    }
    if (significand >= kExact)
    {
        return std::nullopt;
    }
    const double whole = static_cast<double>(significand);
    const double scale = kTens[static_cast<std::size_t>(power < 0 ? -power : power)];
    return power < 0 ? whole / scale : whole * scale;
}

Bignum digits_of(const Decimal &decimal) noexcept
{
    Bignum number;
    for (std::size_t k = 0; k < decimal.count; k += kWordDigits)
    {
        const std::size_t end = (std::min)(k + kWordDigits, decimal.count);
        std::uint64_t chunk = 0;
        for (std::size_t j = k; j < end; ++j)
        {
            chunk = chunk * 10 + decimal.digits[j];
        }
        number.multiply_ten(end - k);
        number.add(chunk);
    }
    return number;
}

Division divided(Bignum numerator, Bignum denominator, std::int64_t shift) noexcept
{
    if (shift < 0)
    {
        numerator.shift(static_cast<std::size_t>(-shift));
    }
    else
    {
        denominator.shift(static_cast<std::size_t>(shift));
    }
    const std::uint64_t quotient = numerator.divide(denominator);
    return {.quotient = quotient, .inexact = !numerator.is_zero()};
}

std::optional<double> assembled(Division division, bool dropped, std::int64_t shift) noexcept
{
    std::uint64_t significand = division.quotient >> 1;
    const bool half = (division.quotient & 1) != 0;
    if (half && (division.inexact || dropped || (significand & 1) != 0))
    {
        ++significand;
    }
    std::int64_t binary = shift + 1;
    if (significand == kExact)
    {
        significand >>= 1;
        ++binary;
    }
    if (significand == 0)
    {
        return std::nullopt;
    }
    if (significand < kHidden)
    {
        return std::bit_cast<double>(significand);
    }
    const std::int64_t biased = binary + kBias;
    if (biased >= kInfinite)
    {
        return std::nullopt;
    }
    const std::uint64_t field = static_cast<std::uint64_t>(biased) << 52;
    return std::bit_cast<double>(field | (significand - kHidden));
}

std::optional<double> exact(const Decimal &decimal) noexcept
{
    Bignum numerator = digits_of(decimal);
    Bignum denominator(1);
    const std::int64_t power = tens_of(decimal);
    if (power < 0)
    {
        denominator.multiply_ten(static_cast<std::size_t>(-power));
    }
    else
    {
        numerator.multiply_ten(static_cast<std::size_t>(power));
    }
    const std::int64_t length =
        static_cast<std::int64_t>(numerator.bits()) - static_cast<std::int64_t>(denominator.bits());
    std::int64_t shift = (std::max)(length - 54, kLeastShift);
    Division division = divided(numerator, denominator, shift);
    if (division.quotient >= kRounded)
    {
        ++shift;
        division = divided(numerator, denominator, shift);
    }
    return assembled(division, decimal.dropped, shift);
}

std::optional<double> magnitude_of(const Decimal &decimal) noexcept
{
    if (decimal.count == 0)
    {
        return 0.0;
    }
    if (decimal.point > kHighest || decimal.point < kLowest)
    {
        return std::nullopt;
    }
    const std::optional<double> quick = fast(decimal);
    return quick ? quick : exact(decimal);
}

}

std::from_chars_result from_decimal(const char *first, const char *last, double &value) noexcept
{
    const bool negative = first != last && *first == '-';
    const char *start = negative ? first + 1 : first;
    Decimal decimal;
    const char *end = mantissa(start, last, decimal);
    if (end == start)
    {
        return {first, std::errc::invalid_argument};
    }
    end = exponent(end, last, decimal);
    trim(decimal);
    const std::optional<double> magnitude = magnitude_of(decimal);
    if (!magnitude)
    {
        return {end, std::errc::result_out_of_range};
    }
    value = negative ? -*magnitude : *magnitude;
    return {end, std::errc()};
}

}
