#include "expr/sparsity/joiner.hpp"
#include "expr/sparsity/radix.hpp"
#include "expr/sparsity/reach.hpp"
#include "graphs.hpp"

#include <gtest/gtest.h>

#include <span>
#include <vector>

namespace
{

using biyos::Joiner;
using biyos::Radix;
using biyos::Range;
using biyos::test::pairs_of;
using biyos::test::Ranges;

constexpr Range kSentinel{.begin = -4, .end = -3};

std::vector<Range> ranges_of(const Ranges &pairs)
{
    std::vector<Range> runs;
    for (const auto &[begin, end] : pairs)
    {
        runs.push_back({.begin = begin, .end = end});
    }
    return runs;
}

Ranges joined(const Ranges &own, const Ranges &runs)
{
    Joiner joiner;
    Radix radix;
    std::vector<Range> into{kSentinel};
    joiner.start(ranges_of(own));
    for (const Range run : ranges_of(runs))
    {
        joiner.push_back(run);
    }
    joiner.finish(radix, into);
    return pairs_of(std::span(into).subspan(1));
}

}

TEST(JoinerTest, Descending)
{
    const Ranges runs{{8, 10}, {6, 8}, {2, 4}, {1, 2}, {1, 2}};
    EXPECT_EQ(joined({}, runs), (Ranges{{1, 4}, {6, 10}}));
}

TEST(JoinerTest, Unordered)
{
    const Ranges runs{{1, 2}, {5, 6}, {3, 5}};
    EXPECT_EQ(joined({}, runs), (Ranges{{1, 2}, {3, 6}}));
}

TEST(JoinerTest, Wider)
{
    const Ranges runs{{8, 10}, {5, 6}, {3, 9}};
    EXPECT_EQ(joined({}, runs), (Ranges{{3, 10}}));
}

TEST(JoinerTest, Nested)
{
    const Ranges runs{{1, 5}, {2, 3}};
    EXPECT_EQ(joined({}, runs), (Ranges{{1, 5}}));
}

TEST(JoinerTest, Own)
{
    const Ranges own{{7, 8}, {0, 1}};
    const Ranges runs{{4, 7}, {1, 2}};
    EXPECT_EQ(joined(own, runs), (Ranges{{0, 2}, {4, 8}}));
}

TEST(JoinerTest, Settled)
{
    const Ranges own{{0, 1}, {7, 8}};
    EXPECT_EQ(joined(own, {}), own);
}

TEST(JoinerTest, Restart)
{
    Joiner joiner;
    Radix radix;
    std::vector<Range> into;
    joiner.start(ranges_of({{5, 6}}));
    joiner.finish(radix, into);
    joiner.start({});
    joiner.push_back({.begin = 2, .end = 3});
    joiner.finish(radix, into);
    EXPECT_EQ(pairs_of(into), (Ranges{{5, 6}, {2, 3}}));
}
