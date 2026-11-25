#include "core/numbers.hpp"
#include "model/attributes/records.hpp"
#include "model/renumber.hpp"
#include "refusal.hpp"
#include "tolerance.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace
{

using biyos::Index;
using biyos::kDeleted;
using biyos::Real;
using biyos::RecordTolerances;
using biyos::test::expect_tolerance;
using biyos::test::kUnknown;
using biyos::test::refusal;

using Indices = std::vector<Index>;
using Values = std::vector<Real>;

constexpr Index kSize = 8;

void set(RecordTolerances &held,
         const Indices &indices,
         const Values &absolute,
         const Values &relative)
{
    held.set("variable", kSize, indices, absolute, relative);
}

void remove(RecordTolerances &held, const Indices &indices)
{
    held.remove("variable", kSize, indices);
}

}

TEST(RecordToleranceTest, Batch)
{
    RecordTolerances held;
    set(held, {3, 1, 3, 0}, {1.0, 2.0, 4.0, 0.5}, {});
    expect_tolerance(held.at(0), 0.5, kUnknown);
    expect_tolerance(held.at(1), 2.0, kUnknown);
    expect_tolerance(held.at(2), kUnknown, kUnknown);
    expect_tolerance(held.at(3), 4.0, kUnknown);
    expect_tolerance(held.at(4), kUnknown, kUnknown);
}

TEST(RecordToleranceTest, Held)
{
    RecordTolerances held;
    set(held, {1, 3}, {1.0, 2.0}, {0.125, 0.25});
    set(held, {4, 3, 2}, {}, {1.5, 0.5, 0.75});
    expect_tolerance(held.at(1), 1.0, 0.125);
    expect_tolerance(held.at(2), kUnknown, 0.75);
    expect_tolerance(held.at(3), 2.0, 0.5);
    expect_tolerance(held.at(4), kUnknown, 1.5);
    set(held, {0}, {3.0}, {});
    expect_tolerance(held.at(0), 3.0, kUnknown);
    expect_tolerance(held.at(1), 1.0, 0.125);
    expect_tolerance(held.at(4), kUnknown, 1.5);
}

TEST(RecordToleranceTest, Repeated)
{
    RecordTolerances held;
    set(held, {3}, {1.0}, {2.0});
    set(held, {3, 3}, {}, {0.5, 0.25});
    expect_tolerance(held.at(3), 1.0, 0.25);
}

TEST(RecordToleranceTest, Remove)
{
    RecordTolerances held;
    set(held, {0, 1, 2, 4}, {1.0, 2.0, 3.0, 4.0}, {});
    remove(held, {4, 1, 3, 1});
    expect_tolerance(held.at(0), 1.0, kUnknown);
    expect_tolerance(held.at(1), kUnknown, kUnknown);
    expect_tolerance(held.at(2), 3.0, kUnknown);
    expect_tolerance(held.at(4), kUnknown, kUnknown);
    set(held, {1}, {}, {0.5});
    expect_tolerance(held.at(1), kUnknown, 0.5);
    expect_tolerance(held.at(2), 3.0, kUnknown);
}

TEST(RecordToleranceTest, RemoveMarked)
{
    RecordTolerances held;
    set(held, {0, 1, 2, 3, 4, 5, 6, 7}, {0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0}, {});
    remove(held, {4, 1});
    expect_tolerance(held.at(1), kUnknown, kUnknown);
    expect_tolerance(held.at(4), kUnknown, kUnknown);
    expect_tolerance(held.at(5), 5.0, kUnknown);
}

TEST(RecordToleranceTest, Renumber)
{
    RecordTolerances held;
    set(held, {0, 2, 3, 4}, {1.0, 2.0, 3.0, 4.0}, {});
    const Indices map{0, kDeleted, kDeleted, 1, 2};
    held.renumber(map);
    expect_tolerance(held.at(0), 1.0, kUnknown);
    expect_tolerance(held.at(1), 3.0, kUnknown);
    expect_tolerance(held.at(2), 4.0, kUnknown);
    expect_tolerance(held.at(3), kUnknown, kUnknown);
    set(held, {2}, {}, {0.5});
    expect_tolerance(held.at(1), 3.0, kUnknown);
    expect_tolerance(held.at(2), 4.0, 0.5);
}

TEST(RecordToleranceTest, Refused)
{
    RecordTolerances held;
    set(held, {2}, {1.0}, {2.0});
    EXPECT_EQ(refusal([&] { set(held, {0, 2, 8}, {3.0, 4.0, 5.0}, {}); }),
              "invalid argument: variable 8: unknown");
    EXPECT_EQ(refusal([&] { set(held, {0, 2}, {}, {kUnknown, 1.0}); }),
              "invalid argument: variable 0: relative tolerance is NaN");
    EXPECT_EQ(refusal([&] { remove(held, {2, 8}); }), "invalid argument: variable 8: unknown");
    expect_tolerance(held.at(0), kUnknown, kUnknown);
    expect_tolerance(held.at(2), 1.0, 2.0);
}
