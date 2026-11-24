#include "core/text/bignum.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>

using biyos::text::Bignum;

namespace
{

constexpr std::uint64_t kAll = ~std::uint64_t{0};
constexpr std::uint64_t kTenNineteen = 10000000000000000000ULL;

class Draws
{
  public:
    std::uint64_t next() noexcept
    {
        state_ += 0x9e3779b97f4a7c15ULL;
        std::uint64_t z = state_;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    std::uint64_t word() noexcept { return next() >> (next() % 64); }

  private:
    std::uint64_t state_ = 7;
};

Bignum power_of_two(std::size_t exponent)
{
    Bignum number(1);
    number.shift(exponent);
    return number;
}

}

TEST(BignumTest, Zero)
{
    const Bignum zero;
    EXPECT_TRUE(zero.is_zero());
    EXPECT_EQ(zero.bits(), 0U);
    EXPECT_EQ(Bignum(0), zero);
    EXPECT_FALSE(Bignum(1).is_zero());
    Bignum product(12345);
    product.multiply(0);
    EXPECT_TRUE(product.is_zero());
    EXPECT_EQ(product, zero);
    Bignum shifted;
    shifted.shift(100);
    EXPECT_TRUE(shifted.is_zero());
}

TEST(BignumTest, Carry)
{
    Bignum number(kAll);
    number.add(1);
    EXPECT_EQ(number.limb(0), 0U);
    EXPECT_EQ(number.limb(1), 1U);
    EXPECT_EQ(number.bits(), 65U);
    number.add(kAll);
    EXPECT_EQ(number.limb(0), kAll);
    EXPECT_EQ(number.limb(1), 1U);
    number.add(1);
    EXPECT_EQ(number.limb(0), 0U);
    EXPECT_EQ(number.limb(1), 2U);
    Bignum ripple = power_of_two(192);
    ripple.add(kAll);
    ripple.add(1);
    EXPECT_EQ(ripple.limb(0), 0U);
    EXPECT_EQ(ripple.limb(1), 1U);
    EXPECT_EQ(ripple.limb(3), 1U);
    Bignum same = power_of_two(128);
    same.add(0);
    EXPECT_EQ(same, power_of_two(128));
}

TEST(BignumTest, Bits)
{
    EXPECT_EQ(Bignum(1).bits(), 1U);
    EXPECT_EQ(Bignum(kAll).bits(), 64U);
    EXPECT_EQ(power_of_two(64).bits(), 65U);
    EXPECT_EQ(power_of_two(4095).bits(), 4096U);
    Bignum ten(1);
    ten.multiply_ten(1123);
    EXPECT_EQ(ten.bits(), 3731U);
}

TEST(BignumTest, Shift)
{
    const Bignum number = power_of_two(70);
    EXPECT_EQ(number.limb(0), 0U);
    EXPECT_EQ(number.limb(1), std::uint64_t{1} << 6);
    Bignum spread(kAll);
    spread.shift(4);
    EXPECT_EQ(spread.limb(0), kAll << 4);
    EXPECT_EQ(spread.limb(1), 15U);
    spread.shift(64);
    EXPECT_EQ(spread.limb(0), 0U);
    EXPECT_EQ(spread.limb(1), kAll << 4);
    EXPECT_EQ(spread.limb(2), 15U);
    EXPECT_EQ(spread.bits(), 132U);
    Bignum still(5);
    still.shift(0);
    EXPECT_EQ(still, Bignum(5));
    Bignum top(1);
    top.shift(4095);
    EXPECT_EQ(top.limb(63), std::uint64_t{1} << 63);
}

TEST(BignumTest, Order)
{
    EXPECT_LT(Bignum(3), Bignum(4));
    EXPECT_GT(Bignum(4), Bignum(3));
    EXPECT_LT(Bignum(kAll), power_of_two(64));
    EXPECT_GT(power_of_two(64), Bignum(kAll));
    Bignum high = power_of_two(64);
    high.add(1);
    Bignum low = power_of_two(64);
    EXPECT_LT(low, high);
    EXPECT_NE(low, high);
    low.add(1);
    EXPECT_EQ(low, high);
    EXPECT_LT(power_of_two(65), power_of_two(64 + 64));
    Bignum upper = power_of_two(128);
    upper.add(5);
    Bignum lower = power_of_two(128);
    lower.add(4);
    EXPECT_LT(lower, upper);
}

TEST(BignumTest, Tens)
{
    for (std::size_t power = 0; power <= 19; ++power)
    {
        Bignum number(1);
        number.multiply_ten(power);
        std::uint64_t expected = 1;
        for (std::size_t k = 0; k < power; ++k)
        {
            expected *= 10;
        }
        EXPECT_EQ(number, Bignum(expected)) << power;
    }
    Bignum expected(3);
    for (std::size_t power = 1; power <= 1200; ++power)
    {
        expected.multiply(10);
        Bignum number(3);
        number.multiply_ten(power);
        EXPECT_EQ(number, expected) << power;
    }
}

TEST(BignumTest, Divide)
{
    Bignum divisor(1);
    divisor.multiply_ten(300);
    divisor.add(12345);
    Draws draws;
    for (int round = 0; round < 200; ++round)
    {
        const std::uint64_t quotient = draws.word();
        const std::uint64_t rest = draws.word();
        Bignum number = divisor;
        number.multiply(quotient);
        number.add(rest);
        EXPECT_EQ(number.divide(divisor), quotient);
        EXPECT_EQ(number, Bignum(rest));
    }
    Bignum exact = divisor;
    exact.shift(63);
    EXPECT_EQ(exact.divide(divisor), std::uint64_t{1} << 63);
    EXPECT_TRUE(exact.is_zero());
    Bignum small(41);
    EXPECT_EQ(small.divide(Bignum(42)), 0U);
    EXPECT_EQ(small, Bignum(41));
    Bignum same(42);
    EXPECT_EQ(same.divide(Bignum(42)), 1U);
    EXPECT_TRUE(same.is_zero());
    Bignum borrowed(2);
    borrowed.shift(64);
    borrowed.add(5);
    borrowed.shift(64);
    Bignum equal(1);
    equal.shift(64);
    equal.add(5);
    equal.shift(64);
    equal.add(1);
    Bignum left(kAll);
    left.shift(64);
    left.add(kAll);
    EXPECT_EQ(borrowed.divide(equal), 1U);
    EXPECT_EQ(borrowed, left);
}

TEST(BignumTest, Powers)
{
    Bignum number(1);
    for (int k = 0; k < 60; ++k)
    {
        number.multiply(kTenNineteen);
    }
    for (int k = 0; k < 60; ++k)
    {
        Bignum divisor(1);
        divisor.multiply_ten(19 * static_cast<std::size_t>(59 - k));
        Bignum rest = number;
        EXPECT_EQ(rest.divide(divisor), kTenNineteen);
        EXPECT_TRUE(rest.is_zero());
        number = divisor;
    }
    EXPECT_EQ(number, Bignum(1));
}

#if defined(__SIZEOF_INT128__)

namespace
{

__extension__ typedef unsigned __int128 Wide;

Wide value_of(const Bignum &number)
{
    EXPECT_LE(number.bits(), 128U);
    return (static_cast<Wide>(number.limb(1)) << 64) | number.limb(0);
}

Bignum bignum_of(Wide value)
{
    Bignum number(static_cast<std::uint64_t>(value >> 64));
    number.shift(64);
    number.add(static_cast<std::uint64_t>(value));
    return number;
}

}

TEST(BignumTest, Products)
{
    Draws draws;
    for (int round = 0; round < 2000; ++round)
    {
        const std::uint64_t a = draws.word();
        const std::uint64_t b = draws.word();
        Bignum number(a);
        number.multiply(b);
        EXPECT_EQ(value_of(number), static_cast<Wide>(a) * b) << a << " " << b;
    }
    Bignum largest(kAll);
    largest.multiply(kAll);
    EXPECT_EQ(value_of(largest), static_cast<Wide>(kAll) * kAll);
}

TEST(BignumTest, Sums)
{
    Draws draws;
    for (int round = 0; round < 2000; ++round)
    {
        const Wide a = (static_cast<Wide>(draws.word() >> 1) << 64) | draws.word();
        const std::uint64_t b = draws.word();
        Bignum number = bignum_of(a);
        number.add(b);
        EXPECT_EQ(value_of(number), a + b);
    }
}

TEST(BignumTest, Shifts)
{
    Draws draws;
    for (int round = 0; round < 2000; ++round)
    {
        const std::uint64_t a = draws.word();
        const std::size_t bits = draws.next() % 65;
        Bignum number(a);
        number.shift(bits);
        EXPECT_EQ(value_of(number), static_cast<Wide>(a) << bits);
        EXPECT_EQ(number.bits(), a == 0 ? 0U : static_cast<std::size_t>(std::bit_width(a)) + bits);
    }
}

TEST(BignumTest, Quotients)
{
    Draws draws;
    for (int round = 0; round < 2000; ++round)
    {
        const Wide a = (static_cast<Wide>(draws.word()) << 64) | draws.word();
        const Wide b = (static_cast<Wide>(draws.word() >> 1) << 64) | draws.word();
        if (b == 0 || a / b > kAll)
        {
            continue;
        }
        Bignum number = bignum_of(a);
        EXPECT_EQ(number.divide(bignum_of(b)), static_cast<std::uint64_t>(a / b));
        EXPECT_EQ(value_of(number), a % b);
        EXPECT_EQ(bignum_of(a) < bignum_of(b), a < b);
    }
}

#endif
