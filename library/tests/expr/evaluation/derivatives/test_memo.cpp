#include "core/numbers.hpp"
#include "expr/sparsity/memo.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <numeric>
#include <vector>

namespace
{

using biyos::Index;
using biyos::Memos;
using biyos::Offset;
using biyos::to_size;

constexpr Index kWide = static_cast<Index>(Memos<Index>::kFlat) + 1;

std::vector<Index> run_of(Index first, Index count)
{
    std::vector<Index> items(to_size(count));
    std::iota(items.begin(), items.end(), first);
    return items;
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
    std::vector<Index> items;
    memos.gather(std::array{first}, items);
    std::ranges::sort(items);
    EXPECT_EQ(items, run_of(0, 2 * kWide));
}
