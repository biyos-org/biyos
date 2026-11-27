#include "core/numbers.hpp"
#include "expr/sparsity/memo.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <span>
#include <vector>

namespace
{

using biyos::Index;
using biyos::Memos;
using biyos::Offset;
using biyos::to_size;

constexpr Index kWide = static_cast<Index>(Memos<Index>::kFlat) + 1;
constexpr std::size_t kLanes = Memos<Index>::kLanes;

std::vector<Index> run_of(Index first, Index count)
{
    std::vector<Index> items(to_size(count));
    std::iota(items.begin(), items.end(), first);
    return items;
}

std::vector<std::vector<Index>> sorted(std::vector<std::vector<Index>> lanes)
{
    for (std::vector<Index> &lane : lanes)
    {
        std::ranges::sort(lane);
    }
    return lanes;
}

std::vector<std::vector<Index>> walked(Memos<Index> &memos, Offset whole, Offset left, Offset right)
{
    std::vector<std::vector<Index>> lanes(kLanes);
    memos.seed(std::array{whole}, 0);
    memos.seed(std::array{left}, 1);
    memos.seed(std::array{right, whole}, kLanes - 1);
    memos.walk(std::span(lanes));
    return sorted(lanes);
}

}

TEST(MemoTest, Shared)
{
    Memos<Index> memos;
    const Offset left = memos.keep(run_of(0, kWide), {});
    const Offset right = memos.keep(run_of(kWide, kWide), {});
    const std::array links{left, right};
    const Offset first = memos.keep({}, links);
    EXPECT_EQ(memos.keep({}, links), first);
    std::vector<std::vector<Index>> lanes(1);
    memos.seed(std::array{first}, 0);
    memos.walk(std::span(lanes));
    EXPECT_EQ(sorted(lanes)[0], run_of(0, 2 * kWide));
}

TEST(MemoTest, Lanes)
{
    Memos<Index> memos;
    const Offset left = memos.keep(run_of(0, kWide), {});
    const Offset right = memos.keep(run_of(kWide, kWide), {});
    const Offset whole = memos.keep(run_of(2 * kWide, kWide), std::array{left, right});
    std::vector<std::vector<Index>> wanted(kLanes);
    wanted[0] = run_of(0, 3 * kWide);
    wanted[1] = run_of(0, kWide);
    wanted[kLanes - 1] = run_of(0, 3 * kWide);
    EXPECT_EQ(walked(memos, whole, left, right), wanted);
    EXPECT_EQ(walked(memos, whole, left, right), wanted);
}
