#include "core/numbers.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

using biyos::Bits;
using biyos::Index;
using biyos::Offset;
using biyos::Real;
using biyos::to_index;
using biyos::to_offset;
using biyos::to_size;

template <typename T>
concept sizable = requires(T v) { to_size(v); };
template <typename T>
concept indexable = requires(T v) { to_index(v); };
template <typename T>
concept offsettable = requires(T v) { to_offset(v); };

TEST(NumberTest, Size)
{
    static_assert(std::is_same_v<decltype(to_size(Index{7})), std::size_t>);
    static_assert(std::is_same_v<decltype(to_size(Offset{9})), std::size_t>);
    EXPECT_EQ(to_size((std::numeric_limits<Index>::max)()), 2147483647U);
    EXPECT_EQ(to_size(Offset{1} << 40), std::size_t{1} << 40);
}

TEST(NumberTest, Index)
{
    static_assert(std::is_same_v<decltype(to_index(std::size_t{3})), Index>);
    EXPECT_EQ(to_index(std::size_t{2147483647}), (std::numeric_limits<Index>::max)());
}

TEST(NumberTest, Offset)
{
    static_assert(std::is_same_v<decltype(to_offset(std::size_t{5})), Offset>);
    EXPECT_EQ(to_offset(std::size_t{1} << 40), Offset{1} << 40);
}

TEST(NumberTest, Constant)
{
    static_assert(to_size(Index{7}) == 7U);
    static_assert(to_size(Offset{9}) == 9U);
    static_assert(to_index(std::size_t{3}) == 3);
    static_assert(to_offset(std::size_t{5}) == 5);
    SUCCEED();
}

TEST(NumberTest, Noexcept)
{
    static_assert(noexcept(to_size(Index{0})));
    static_assert(noexcept(to_size(Offset{0})));
    static_assert(noexcept(to_index(std::size_t{0})));
    static_assert(noexcept(to_offset(std::size_t{0})));
    SUCCEED();
}

TEST(NumberTest, Exact)
{
    static_assert(sizable<Index> && sizable<Offset>);
    static_assert(!sizable<std::size_t> && !sizable<unsigned> && !sizable<bool>);
    static_assert(!sizable<char> && !sizable<short> && !sizable<std::uint8_t>);
    static_assert(indexable<std::size_t> && !indexable<int> && !indexable<Offset>);
    static_assert(offsettable<std::size_t> && !offsettable<int> && !offsettable<Offset>);
    SUCCEED();
}

TEST(NumberTest, Bits)
{
    static_assert(sizeof(Bits) == sizeof(Real));
    static_assert(std::is_unsigned_v<Bits>);
    SUCCEED();
}
