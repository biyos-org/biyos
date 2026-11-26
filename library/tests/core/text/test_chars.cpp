#include "core/text/chars.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>

using biyos::text::from_chars;
using biyos::text::from_decimal;

namespace
{

constexpr double kUntouched = -7.25;
constexpr int kFixed = 1080;
constexpr double kLargest = (std::numeric_limits<double>::max)();
constexpr double kLeast = std::numeric_limits<double>::denorm_min();

struct Read
{
    std::uint64_t bits;
    std::ptrdiff_t used;
    std::errc ec;
};

Read ours(std::string_view text)
{
    double value = kUntouched;
    const std::from_chars_result result =
        from_decimal(text.data(), text.data() + text.size(), value);
    return {std::bit_cast<std::uint64_t>(value), result.ptr - text.data(), result.ec};
}

void expect_same(std::string_view text)
{
#if defined(__cpp_lib_to_chars)
    double value = kUntouched;
    const std::from_chars_result result =
        std::from_chars(text.data(), text.data() + text.size(), value);
    const Read read = ours(text);
    if (result.ec != std::errc::result_out_of_range)
    {
        EXPECT_EQ(read.bits, std::bit_cast<std::uint64_t>(value)) << text;
    }
    EXPECT_EQ(read.used, result.ptr - text.data()) << text;
    EXPECT_EQ(read.ec, result.ec) << text;
#else
    static_cast<void>(text);
#endif
}

void expect_value(std::string_view text, double expected, std::size_t used)
{
    const Read read = ours(text);
    EXPECT_EQ(read.bits, std::bit_cast<std::uint64_t>(expected)) << text;
    EXPECT_EQ(read.used, static_cast<std::ptrdiff_t>(used)) << text;
    EXPECT_EQ(read.ec, std::errc()) << text;
    expect_same(text);
}

void expect_refused(std::string_view text, std::errc ec, std::size_t used)
{
    const Read read = ours(text);
    EXPECT_EQ(read.bits, std::bit_cast<std::uint64_t>(kUntouched)) << text;
    EXPECT_EQ(read.used, static_cast<std::ptrdiff_t>(used)) << text;
    EXPECT_EQ(read.ec, ec) << text;
    expect_same(text);
}

void expect_value(const std::string &text, double expected)
{
    expect_value(text, expected, text.size());
}

void expect_range(const std::string &text)
{
    expect_refused(text, std::errc::result_out_of_range, text.size());
}

std::string exact(double value)
{
    std::array<char, 1500> buffer{};
    const std::to_chars_result result = std::to_chars(buffer.data(),
                                                      buffer.data() + buffer.size(),
                                                      value,
                                                      std::chars_format::fixed,
                                                      kFixed);
    std::string text(buffer.data(), result.ptr);
    text.insert(0, 400 - text.find('.'), '0');
    return text;
}

std::string sum(const std::string &a, const std::string &b)
{
    std::string total = a;
    int carry = 0;
    for (std::size_t k = total.size(); k-- > 0;)
    {
        if (total[k] == '.')
        {
            continue;
        }
        const int digit = (a[k] - '0') + (b[k] - '0') + carry;
        total[k] = static_cast<char>('0' + digit % 10);
        carry = digit / 10;
    }
    return total;
}

std::string halved(const std::string &a)
{
    std::string half = a + "0";
    int rest = 0;
    for (char &c : half)
    {
        if (c == '.')
        {
            continue;
        }
        const int digit = rest * 10 + (c - '0');
        c = static_cast<char>('0' + digit / 2);
        rest = digit % 2;
    }
    return half;
}

std::string halfway(double a, double b)
{
    return halved(sum(exact(a), exact(b)));
}

std::string above(const std::string &a)
{
    return a + std::string(200, '0') + "1";
}

std::string below(const std::string &a)
{
    std::string less = a + "0";
    std::size_t k = less.size();
    while (k-- > 0 && (less[k] == '0' || less[k] == '.'))
    {
        less[k] = less[k] == '.' ? '.' : '9';
    }
    --less[k];
    return less;
}

double even(double a, double b)
{
    return (std::bit_cast<std::uint64_t>(a) & 1) == 0 ? a : b;
}

void expect_halfway(double a)
{
    const double b = std::nextafter(a, kLargest * 2);
    const std::string middle = halfway(a, b);
    expect_value(middle, even(a, b));
    expect_value(above(middle), b);
    expect_value(below(middle), a);
    expect_value(exact(a), a);
    expect_value(exact(b), b);
}

}

TEST(CharsTest, Grammar)
{
    expect_value(".5", 0.5, 2);
    expect_value("5.", 5.0, 2);
    expect_value("1e", 1.0, 1);
    expect_value("1e+", 1.0, 1);
    expect_value("1E-", 1.0, 1);
    expect_value("1ex", 1.0, 1);
    expect_value("1E2", 100.0, 3);
    expect_value("1e+2", 100.0, 4);
    expect_value("25e-1x", 2.5, 5);
    expect_value("-.5e1", -5.0, 5);
    expect_value("1.5.3", 1.5, 3);
    expect_value("0x10", 0.0, 1);
    expect_value("12 3", 12.0, 2);
    expect_value("-0", -0.0, 2);
    expect_value("0", 0.0, 1);
    expect_value("0.e5", 0.0, 4);
    expect_value("-0.000e-999", -0.0, 11);
    expect_value("0e99999999999999999999", 0.0, 22);
    expect_refused("", std::errc::invalid_argument, 0);
    expect_refused("-", std::errc::invalid_argument, 0);
    expect_refused("+1", std::errc::invalid_argument, 0);
    expect_refused(".", std::errc::invalid_argument, 0);
    expect_refused("-.", std::errc::invalid_argument, 0);
    expect_refused(".e5", std::errc::invalid_argument, 0);
    expect_refused("e5", std::errc::invalid_argument, 0);
    expect_refused("--1", std::errc::invalid_argument, 0);
    expect_refused("x1", std::errc::invalid_argument, 0);
}

TEST(CharsTest, Bounded)
{
    const std::string_view text = "12345";
    double value = kUntouched;
    const std::from_chars_result result = from_decimal(text.data(), text.data() + 3, value);
    EXPECT_EQ(value, 123.0);
    EXPECT_EQ(result.ptr, text.data() + 3);
    const std::string_view exponent = "1e5";
    from_decimal(exponent.data(), exponent.data() + 2, value);
    EXPECT_EQ(value, 1.0);
    const std::string_view point = "1.5";
    from_decimal(point.data(), point.data() + 2, value);
    EXPECT_EQ(value, 1.0);
    const std::string_view sign = "1e-5";
    EXPECT_EQ(from_decimal(sign.data(), sign.data() + 3, value).ptr, sign.data() + 1);
}

TEST(CharsTest, Zeros)
{
    expect_value("0000000000000000000000000000001.5000000000000000000000000000", 1.5);
    expect_value("1" + std::string(900, '0') + "e-900", 1.0);
    expect_value("0." + std::string(1000, '0') + "1e1001", 1.0);
    expect_value("0." + std::string(1000, '0') + "1e700", 1e-301);
    expect_value(std::string(1000, '0') + "7", 7.0);
    expect_value("1." + std::string(2000, '0'), 1.0);
    expect_value("123" + std::string(300, '0'), 123e300);
    expect_value("0." + std::string(330, '0') + "1e10", 1e-321);
}

TEST(CharsTest, Fast)
{
    expect_value("9007199254740991", 9007199254740991.0);
    expect_value("9007199254740993", 9007199254740992.0);
    expect_value("9007199254740995", 9007199254740996.0);
    expect_value("1e22", 1e22);
    expect_value("1e23", 1e23);
    expect_value("1e-22", 1e-22);
    expect_value("1e-23", 1e-23);
    expect_value("9007199254740991e22", 9007199254740991e22);
    expect_value("9007199254740991e23", 9007199254740991e23);
    expect_value("9007199254740991e-22", 9007199254740991e-22);
    expect_value("9007199254740991e-23", 9007199254740991e-23);
    expect_value("1234567890123456789", 1234567890123456789.0);
    expect_value("12345678901234567891", 12345678901234567891.0);
    expect_value("0.1", 0.1);
    expect_value("0.3", 0.3);
    expect_value("3.141592653589793", 3.141592653589793);
    expect_value("2.2250738585072014e-308", 2.2250738585072014e-308);
    expect_value("2.2250738585072011e-308", 2.2250738585072011e-308);
    expect_value("17121788563964217e3", 17121788563964217e3);
    expect_value("15870012218397279e2", 15870012218397279e2);
    expect_value("12356015806419191e-1", 12356015806419191e-1);
    expect_value("9925701821262185e-3", 9925701821262185e-3);
}

TEST(CharsTest, Tens)
{
    for (int power = -330; power <= 315; ++power)
    {
        expect_same("1e" + std::to_string(power));
        expect_same("-9.999999999999999999999e" + std::to_string(power));
        expect_same("5e" + std::to_string(power));
    }
    expect_value("1e308", 1e308);
    expect_value("1e-307", 1e-307);
    expect_value("1e-323", 1e-323);
    expect_range("1e309");
    expect_range("1e-324");
}

TEST(CharsTest, Range)
{
    expect_range("1e400");
    expect_range("-1e400");
    expect_range("1e-400");
    expect_range("-1e-400");
    expect_range("2e-324");
    expect_range("2.4703282292062327e-324");
    expect_range("1e99999999999999999999");
    expect_range("1e-99999999999999999999");
    expect_range("1e18446744073709551616");
    expect_range("1e-18446744073709551616");
    expect_range("0." + std::string(400, '0') + "1");
    expect_value("2.4703282292062328e-324", kLeast);
    expect_value("3e-324", kLeast);
    expect_value("4e-324", kLeast);
    expect_value("5e-324", kLeast);
    expect_value("-4e-324", -kLeast);
    expect_value("7.4e-324", kLeast);
    expect_value("7.5e-324", 2 * kLeast);
    expect_value("1.7976931348623157e308", kLargest);
    expect_value("1.7976931348623158e308", kLargest);
    expect_range("1.7976931348623159e308");
    expect_range("1.8e308");
    const std::string past = sum(exact(kLargest), exact(0x1p970));
    expect_range(past);
    expect_range(above(past));
    expect_value(below(past), kLargest);
    expect_value("-" + below(past), -kLargest);
    expect_refused("1e400x", std::errc::result_out_of_range, 5);
}

TEST(CharsTest, Subnormal)
{
    const std::string least = halfway(0.0, kLeast);
    expect_range(least);
    expect_range(below(least));
    expect_value(above(least), kLeast);
    expect_halfway(kLeast);
    expect_halfway(2 * kLeast);
    expect_halfway(3 * kLeast);
    expect_halfway(0x1p-1022 - kLeast);
    expect_halfway(0x1p-1022 - 2 * kLeast);
    expect_halfway(0x1p-1022);
    expect_halfway(0x0.8p-1022);
}

TEST(CharsTest, Halfway)
{
    expect_halfway(1.0);
    expect_halfway(std::nextafter(1.0, 0.0));
    expect_halfway(0.1);
    expect_halfway(1e23);
    expect_halfway(9007199254740992.0);
    expect_halfway(0x1p-1021);
    expect_halfway(0x1.fffffffffffffp-1022);
    expect_halfway(std::nextafter(kLargest, 0.0));
    expect_halfway(0x1p1000);
    expect_halfway(0x1.123456789abcdp-500);
    expect_halfway(0x1.fedcba9876543p+700);
    expect_halfway(1e-300);
    expect_halfway(123456.789);
}

TEST(CharsTest, Long)
{
    const std::string middle = halfway(1.0, std::nextafter(1.0, 2.0));
    const std::string tie = middle.substr(0, middle.find_last_not_of('0') + 1);
    expect_value(tie + std::string(800, '0'), 1.0);
    expect_value(tie + std::string(800, '0') + "1", std::nextafter(1.0, 2.0));
    expect_value(tie + std::string(5000, '0') + "1e0", std::nextafter(1.0, 2.0));
    const std::string digits(800, '7');
    expect_value("0." + digits, 0.7777777777777778);
    expect_value(digits + "e-800", 0.7777777777777778);
    expect_value("1" + std::string(799, '0') + "e-799", 1.0);
    expect_value("1" + std::string(799, '0') + "1e-800", 1.0);
    const std::string nines(900, '9');
    expect_value("0." + nines, 1.0);
    expect_same(nines + "e-1209");
    expect_same(nines + "e-1224");
    expect_same(nines + "e-1100");
    expect_value("0." + std::string(1100, '0') + "1e1200", 1e99);
    expect_value("1" + std::string(1100, '0') + "e-1200", 1e-100);
    expect_value("144115188075858000." + std::string(900, '0') + "1", 0x1.0000000000043p+57);
}

TEST(CharsTest, Forward)
{
    const std::array<std::string_view, 6> texts = {"1.5e3x", "-0", "+1", "1e400", "5e-324", ".e1"};
    for (const std::string_view text : texts)
    {
        double value = kUntouched;
        double decimal = kUntouched;
        const char *last = text.data() + text.size();
        const std::from_chars_result result = from_chars(text.data(), last, value);
        const std::from_chars_result expected = from_decimal(text.data(), last, decimal);
        if (result.ec != std::errc::result_out_of_range)
        {
            EXPECT_EQ(std::bit_cast<std::uint64_t>(value), std::bit_cast<std::uint64_t>(decimal))
                << text;
        }
        EXPECT_EQ(result.ptr, expected.ptr) << text;
        EXPECT_EQ(result.ec, expected.ec) << text;
    }
}
