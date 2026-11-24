#include "core/index.hpp"
#include "expr/forms/terms.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{

using biyos::add_alike;
using biyos::Index;
using biyos::LinearTerm;
using biyos::QuadraticTerm;

constexpr double kAbsorber = 0x1p54;

std::uint64_t bits(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

void expect_linear(const std::vector<LinearTerm> &found, const std::vector<LinearTerm> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        EXPECT_EQ(found[k].variable, wanted[k].variable);
        EXPECT_EQ(bits(found[k].coefficient), bits(wanted[k].coefficient));
    }
}

void expect_quadratic(const std::vector<QuadraticTerm> &found,
                      const std::vector<QuadraticTerm> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        EXPECT_EQ(found[k].row, wanted[k].row);
        EXPECT_EQ(found[k].column, wanted[k].column);
        EXPECT_EQ(bits(found[k].coefficient), bits(wanted[k].coefficient));
    }
}

}

TEST(TermTest, LinearOrder)
{
    std::vector<LinearTerm> terms{
        {.variable = 2, .coefficient = 1.0},
        {.variable = 3, .coefficient = 2.0},
        {.variable = 0, .coefficient = 3.0},
    };
    add_alike(terms);
    expect_linear(terms,
                  {
                      {.variable = 0, .coefficient = 3.0},
                      {.variable = 2, .coefficient = 1.0},
                      {.variable = 3, .coefficient = 2.0},
                  });
}

TEST(TermTest, PairOrder)
{
    std::vector<QuadraticTerm> terms{
        {.row = 1, .column = 0, .coefficient = 2.0},
        {.row = 2, .column = 1, .coefficient = 1.0},
        {.row = 0, .column = 1, .coefficient = 3.0},
        {.row = 0, .column = 0, .coefficient = 4.0},
    };
    add_alike(terms);
    expect_quadratic(terms,
                     {
                         {.row = 0, .column = 0, .coefficient = 4.0},
                         {.row = 0, .column = 1, .coefficient = 5.0},
                         {.row = 1, .column = 2, .coefficient = 1.0},
                     });
}

TEST(TermTest, StableAdding)
{
    const Index n = 16;
    const std::array<double, 5> absorbed{kAbsorber, 1.0, 1.0, 1.0, -kAbsorber};
    std::vector<LinearTerm> terms;
    for (Index k = 1; k < n; ++k)
    {
        terms.push_back({.variable = k, .coefficient = 1.0});
        if (k % 3 == 0)
        {
            const double weight = absorbed[static_cast<std::size_t>(k / 3 - 1)];
            terms.push_back({.variable = 0, .coefficient = weight});
        }
    }
    add_alike(terms);
    ASSERT_EQ(terms.size(), static_cast<std::size_t>(n));
    EXPECT_EQ(terms[0].variable, 0);
    EXPECT_EQ(bits(terms[0].coefficient), bits(0.0));
}

TEST(TermTest, ZeroKept)
{
    std::vector<LinearTerm> linear{
        {.variable = 0, .coefficient = 1.5},
        {.variable = 1, .coefficient = 0.0},
        {.variable = 0, .coefficient = -1.5},
    };
    add_alike(linear);
    expect_linear(linear,
                  {
                      {.variable = 0, .coefficient = 0.0},
                      {.variable = 1, .coefficient = 0.0},
                  });
    std::vector<QuadraticTerm> quadratic{
        {.row = 0, .column = 1, .coefficient = 2.0},
        {.row = 1, .column = 0, .coefficient = -2.0},
    };
    add_alike(quadratic);
    expect_quadratic(quadratic, {{.row = 0, .column = 1, .coefficient = 0.0}});
}

TEST(TermTest, NoTerms)
{
    std::vector<LinearTerm> linear;
    std::vector<QuadraticTerm> quadratic;
    add_alike(linear);
    add_alike(quadratic);
    EXPECT_TRUE(linear.empty());
    EXPECT_TRUE(quadratic.empty());
}
