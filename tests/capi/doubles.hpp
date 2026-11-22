#pragma once

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

namespace biyos::test
{

inline void expect_doubles(const std::vector<double> &found, const std::vector<double> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        EXPECT_DOUBLE_EQ(found[k], wanted[k]) << "entry " << k;
    }
}

}
