#include "core/text/bignum.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <compare>
#include <cstddef>
#include <cstdint>

namespace biyos::text
{

namespace
{

constexpr std::uint64_t kHalf = 0xffffffff;
constexpr std::size_t kWordTens = 19;

struct Wide
{
    std::uint64_t low;
    std::uint64_t high;
};

Wide product(std::uint64_t a, std::uint64_t b) noexcept
{
    const std::uint64_t low = (a & kHalf) * (b & kHalf);
    const std::uint64_t left = (a >> 32) * (b & kHalf);
    const std::uint64_t right = (a & kHalf) * (b >> 32);
    const std::uint64_t high = (a >> 32) * (b >> 32);
    const std::uint64_t middle = (low >> 32) + (left & kHalf) + (right & kHalf);
    return {
        .low = (middle << 32) | (low & kHalf),
        .high = high + (left >> 32) + (right >> 32) + (middle >> 32),
    };
}

constexpr std::array<std::uint64_t, kWordTens + 1> tens() noexcept
{
    std::array<std::uint64_t, kWordTens + 1> table{};
    table[0] = 1;
    for (std::size_t k = 1; k < table.size(); ++k)
    {
        table[k] = table[k - 1] * 10;
    }
    return table;
}

constexpr std::array<std::uint64_t, kWordTens + 1> kTens = tens();

}

Bignum::Bignum(std::uint64_t value) noexcept : limbs_{value}, size_(value == 0 ? 0 : 1) {}

void Bignum::add(std::uint64_t value) noexcept
{
    for (std::size_t k = 0; value != 0 && k < kLimbs; ++k)
    {
        limbs_[k] += value;
        value = limbs_[k] < value ? 1 : 0;
        size_ = (std::max)(size_, k + 1);
    }
}

void Bignum::multiply(std::uint64_t factor) noexcept
{
    std::uint64_t carry = 0;
    for (std::size_t k = 0; k < size_; ++k)
    {
        const Wide wide = product(limbs_[k], factor);
        limbs_[k] = wide.low + carry;
        carry = wide.high + (limbs_[k] < carry ? 1 : 0);
    }
    if (carry != 0 && size_ < kLimbs)
    {
        limbs_[size_++] = carry;
    }
    trim();
}

void Bignum::multiply_ten(std::size_t power) noexcept
{
    for (; power > kWordTens; power -= kWordTens)
    {
        multiply(kTens[kWordTens]);
    }
    multiply(kTens[power]);
}

void Bignum::shift(std::size_t bits) noexcept
{
    const std::size_t words = bits / 64;
    const std::size_t rest = bits % 64;
    const std::size_t end = (std::min)(size_ + words + 1, kLimbs);
    for (std::size_t k = end; k-- > 0;)
    {
        limbs_[k] = k < words ? 0 : spliced(k - words, rest);
    }
    size_ = end;
    trim();
}

std::uint64_t Bignum::divide(const Bignum &divisor) noexcept
{
    if (*this < divisor)
    {
        return 0;
    }
    std::uint64_t quotient = 0;
    for (std::size_t s = bits() - divisor.bits() + 1; s-- > 0;)
    {
        Bignum shifted = divisor;
        shifted.shift(s);
        if (shifted <= *this)
        {
            subtract(shifted);
            quotient |= std::uint64_t{1} << s;
        }
    }
    return quotient;
}

std::size_t Bignum::bits() const noexcept
{
    return size_ == 0
               ? 0
               : (size_ - 1) * 64 + static_cast<std::size_t>(std::bit_width(limbs_[size_ - 1]));
}

bool Bignum::is_zero() const noexcept
{
    return size_ == 0;
}

std::uint64_t Bignum::limb(std::size_t index) const noexcept
{
    return limbs_[index];
}

std::strong_ordering operator<=>(const Bignum &a, const Bignum &b) noexcept
{
    if (a.size_ != b.size_)
    {
        return a.size_ <=> b.size_;
    }
    for (std::size_t k = a.size_; k-- > 0;)
    {
        if (a.limbs_[k] != b.limbs_[k])
        {
            return a.limbs_[k] <=> b.limbs_[k];
        }
    }
    return std::strong_ordering::equal;
}

bool operator==(const Bignum &a, const Bignum &b) noexcept
{
    return (a <=> b) == 0;
}

void Bignum::subtract(const Bignum &other) noexcept
{
    std::uint64_t borrow = 0;
    for (std::size_t k = 0; k < size_; ++k)
    {
        const std::uint64_t left = limbs_[k];
        const std::uint64_t right = other.limbs_[k];
        limbs_[k] = left - right - borrow;
        borrow = left < right || (left == right && borrow != 0) ? 1 : 0;
    }
    trim();
}

std::uint64_t Bignum::spliced(std::size_t index, std::size_t rest) const noexcept
{
    const std::uint64_t low = rest == 0 || index == 0 ? 0 : limbs_[index - 1] >> (64 - rest);
    return (limbs_[index] << rest) | low;
}

void Bignum::trim() noexcept
{
    while (size_ > 0 && limbs_[size_ - 1] == 0)
    {
        --size_;
    }
}

}
