#pragma once

#include "core/numbers.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace biyos::test
{

using Points = std::vector<std::vector<Real>>;

constexpr std::array kLanes{std::size_t{1}, std::size_t{3}, std::size_t{16}};

inline std::vector<Real> interleave(const Points &points)
{
    const std::size_t lanes = points.size();
    std::vector<Real> items(points[0].size() * lanes);
    for (std::size_t l = 0; l < lanes; ++l)
    {
        for (std::size_t k = 0; k < points[l].size(); ++k)
        {
            items[k * lanes + l] = points[l][k];
        }
    }
    return items;
}

inline std::vector<Real> lane(std::span<const Real> items, std::size_t l, std::size_t lanes)
{
    std::vector<Real> mine;
    for (std::size_t k = l; k < items.size(); k += lanes)
    {
        mine.push_back(items[k]);
    }
    return mine;
}

}
