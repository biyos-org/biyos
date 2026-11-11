#include "expr/radix.hpp"

#include "core/index.hpp"
#include "expr/reach.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

namespace biyos
{

namespace
{

constexpr int kDigit = 8;
constexpr std::size_t kBuckets = std::size_t{1} << kDigit;

std::size_t digit_of(const Range &item, Offset low, int shift) noexcept
{
    const auto key = static_cast<std::uint64_t>(item.begin - low);
    return static_cast<std::size_t>(key >> shift) & (kBuckets - 1);
}

}

void Radix::sort(std::vector<Range> &items)
{
    if (items.size() < kBuckets)
    {
        std::ranges::sort(items, {}, &Range::begin);
        return;
    }
    const auto [low, high] = std::ranges::minmax(items, {}, &Range::begin);
    const int bits = std::bit_width(static_cast<std::uint64_t>(high.begin - low.begin));
    spare_.resize(items.size());
    for (int shift = 0; shift < bits; shift += kDigit)
    {
        pass(items, low.begin, shift);
        items.swap(spare_);
    }
}

void Radix::pass(const std::vector<Range> &items, Offset low, int shift)
{
    std::array<std::size_t, kBuckets> counts{};
    for (const Range &item : items)
    {
        ++counts[digit_of(item, low, shift)];
    }
    std::exclusive_scan(counts.begin(), counts.end(), counts.begin(), std::size_t{0});
    for (const Range &item : items)
    {
        spare_[counts[digit_of(item, low, shift)]++] = item;
    }
}

}
