#include "core/error.hpp"
#include "core/index.hpp"
#include "model/compare.hpp"
#include "model/model.hpp"
#include "model/overrides.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::Complement;
using biyos::Index;
using biyos::Model;
using biyos::Offset;
using biyos::Override;
using biyos::test::failure;
using biyos::test::refusal;

using Indices = std::vector<Index>;
using Reals = std::vector<double>;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

Model with_functions(std::size_t n)
{
    Model model;
    const std::vector<double> constants(n, 0.0);
    const std::vector<Offset> starts(n + 1, 0);
    model.add_functions(constants, starts, {});
    return model;
}

Complement pair(Index function, Index partner, double lower, double upper)
{
    return Complement{.function = function, .partner = partner, .lower = lower, .upper = upper};
}

void add(Model &model, const Complement &complement)
{
    model.add_complements(std::span(&complement, 1));
}

std::string add_refusal(Model &model, const Complement &complement)
{
    return refusal([&] { add(model, complement); });
}

std::string set_refusal(Model &model, Index index, const Complement &complement)
{
    return refusal([&] { model.set_complements(std::span(&index, 1), std::span(&complement, 1)); });
}

void expect_complement(const Complement &found, const Complement &wanted)
{
    EXPECT_EQ(found.function, wanted.function);
    EXPECT_EQ(found.partner, wanted.partner);
    EXPECT_DOUBLE_EQ(found.lower, wanted.lower);
    EXPECT_DOUBLE_EQ(found.upper, wanted.upper);
}

Model with_complements(std::size_t n)
{
    Model model = with_functions(1);
    model.add_complements(std::vector<Complement>(n, pair(0, 0, 0.0, kInfinity)));
    return model;
}

std::string tolerance_refusal(Model &model,
                              const Indices &indices,
                              const Reals &absolute,
                              const Reals &relative)
{
    return refusal([&] { model.set_complement_tolerances(indices, absolute, relative); });
}

void expect_part(double found, double wanted)
{
    EXPECT_EQ(std::bit_cast<std::uint64_t>(found), std::bit_cast<std::uint64_t>(wanted));
}

void expect_tolerance(const Override &found, double absolute, double relative)
{
    expect_part(found.absolute, absolute);
    expect_part(found.relative, relative);
}

}

TEST(ComplementTest, Nonnegative)
{
    Model model = with_functions(2);
    const Complement classical = pair(0, 1, 0.0, kInfinity);
    add(model, classical);
    ASSERT_EQ(model.n_complements(), 1);
    expect_complement(model.complement(0), classical);
}

TEST(ComplementTest, FiniteRange)
{
    Model model = with_functions(2);
    const std::vector<Complement> added{pair(1, 0, -1.0, 2.0), pair(0, 1, 3.0, 3.0)};
    model.add_complements(added);
    ASSERT_EQ(model.n_complements(), 2);
    expect_complement(model.complement(0), added[0]);
    expect_complement(model.complement(1), added[1]);
}

TEST(ComplementTest, BothInfinite)
{
    Model model = with_functions(2);
    const std::vector<Complement> added{
        pair(0, 1, -kInfinity, kInfinity),
        pair(0, 1, -kInfinity, 4.0),
    };
    model.add_complements(added);
    expect_complement(model.complement(0), added[0]);
    expect_complement(model.complement(1), added[1]);
}

TEST(ComplementTest, SameFunction)
{
    Model model = with_functions(1);
    const Complement same = pair(0, 0, 0.0, kInfinity);
    add(model, same);
    expect_complement(model.complement(0), same);
}

TEST(ComplementTest, SharedFunction)
{
    Model model = with_functions(3);
    const std::vector<Complement> added{pair(1, 0, 0.0, kInfinity), pair(2, 1, 0.0, 1.0)};
    model.add_complements(added);
    ASSERT_EQ(model.n_complements(), 2);
    expect_complement(model.complement(0), added[0]);
    expect_complement(model.complement(1), added[1]);
    EXPECT_EQ(model.n_functions(), 3);
}

TEST(ComplementTest, Reversed)
{
    Model model = with_functions(1);
    const Complement reversed = pair(0, 0, 5.0, 1.0);
    add(model, reversed);
    expect_complement(model.complement(0), reversed);
}

TEST(ComplementTest, Copy)
{
    Model model = with_functions(2);
    model.add_complements(
        std::vector<Complement>{pair(0, 1, 0.0, kInfinity), pair(1, 1, -1.0, 1.0)});
    const Model copy = model;
    EXPECT_EQ(biyos::difference(model, copy), "");
    Model changed = model;
    const Index index = 1;
    const Complement other = pair(1, 0, -1.0, 1.0);
    changed.set_complements(std::span(&index, 1), std::span(&other, 1));
    EXPECT_EQ(biyos::difference(model, changed), "complement 1: partner 1 against 0");
    EXPECT_EQ(biyos::difference(model, copy), "");
}

TEST(ComplementTest, UnknownFunction)
{
    Model model = with_functions(2);
    add(model, pair(1, 0, 0.0, 1.0));
    const std::string refused = "invalid argument: complement 1: ";
    EXPECT_EQ(add_refusal(model, pair(2, 0, 0.0, 1.0)), refused + "function 2 is unknown");
    EXPECT_EQ(add_refusal(model, pair(0, 2, 0.0, 1.0)), refused + "function 2 is unknown");
    EXPECT_EQ(add_refusal(model, pair(-1, 0, kNan, 1.0)), refused + "function -1 is unknown");
    EXPECT_EQ(add_refusal(model, pair(0, -1, kNan, 1.0)), refused + "function -1 is unknown");
    EXPECT_EQ(set_refusal(model, 0, pair(0, 9, 0.0, 1.0)),
              "invalid argument: complement 0: function 9 is unknown");
    EXPECT_EQ(model.n_complements(), 1);
    expect_complement(model.complement(0), pair(1, 0, 0.0, 1.0));
    Model empty;
    EXPECT_EQ(add_refusal(empty, pair(0, 0, 0.0, 1.0)),
              "invalid argument: complement 0: function 0 is unknown");
}

TEST(ComplementTest, Infinities)
{
    Model model = with_functions(1);
    const std::string refused = "invalid argument: complement 0: ";
    EXPECT_EQ(add_refusal(model, pair(0, 0, kInfinity, kInfinity)),
              refused + "lower bound is infinity");
    EXPECT_EQ(add_refusal(model, pair(0, 0, -kInfinity, -kInfinity)),
              refused + "upper bound is minus infinity");
    EXPECT_EQ(model.n_complements(), 0);
}

TEST(ComplementTest, NanBounds)
{
    Model model = with_functions(1);
    const std::string refused = "invalid argument: complement 0: ";
    EXPECT_EQ(add_refusal(model, pair(0, 0, kNan, 1.0)), refused + "lower bound is NaN");
    EXPECT_EQ(add_refusal(model, pair(0, 0, 0.0, kNan)), refused + "upper bound is NaN");
    EXPECT_EQ(model.n_complements(), 0);
}

TEST(ComplementTest, Unchanged)
{
    Model model = with_functions(1);
    const Complement kept = pair(0, 0, 0.0, 1.0);
    add(model, kept);
    const std::vector<Complement> refused{pair(0, 0, 1.0, 2.0), pair(0, 0, kNan, 2.0)};
    EXPECT_NE(refusal([&] { model.add_complements(refused); }), "");
    const std::vector<Index> indices{0, 1};
    const std::vector<Complement> changed{pair(0, 0, 2.0, 3.0), kept};
    EXPECT_NE(refusal([&] { model.set_complements(indices, changed); }), "");
    const std::vector<Index> again{0, 0};
    const std::vector<Complement> nan{pair(0, 0, 2.0, 3.0), pair(0, 0, kNan, 1.0)};
    EXPECT_NE(refusal([&] { model.set_complements(again, nan); }), "");
    ASSERT_EQ(model.n_complements(), 1);
    expect_complement(model.complement(0), kept);
}

TEST(ComplementTest, LastWins)
{
    Model model = with_functions(2);
    model.add_complements(std::vector<Complement>{pair(0, 1, 0.0, 1.0), pair(0, 1, 0.0, 1.0)});
    const std::vector<Index> indices{1, 0, 1};
    const std::vector<Complement> changed{
        pair(1, 0, 1.0, 2.0),
        pair(0, 0, 3.0, 4.0),
        pair(1, 1, -kInfinity, 6.0),
    };
    model.set_complements(indices, changed);
    expect_complement(model.complement(0), changed[1]);
    expect_complement(model.complement(1), changed[2]);
}

TEST(ComplementTest, BadIndex)
{
    Model model = with_functions(1);
    add(model, pair(0, 0, 0.0, 1.0));
    EXPECT_EQ(refusal([&] { model.complement(-1); }), "invalid argument: complement -1: unknown");
    EXPECT_EQ(refusal([&] { model.complement(1); }), "invalid argument: complement 1: unknown");
    EXPECT_EQ(set_refusal(model, 1, pair(0, 0, 0.0, 1.0)),
              "invalid argument: complement 1: unknown");
    EXPECT_EQ(set_refusal(model, 0, pair(0, 0, kInfinity, 1.0)),
              "invalid argument: complement 0: lower bound is infinity");
}

TEST(ComplementTest, ReadRange)
{
    Model model = with_functions(3);
    const std::vector<Complement> added{
        pair(2, 0, 0.0, 1.0),
        pair(0, 1, 2.0, 3.0),
        pair(1, 2, 4.0, 5.0),
    };
    model.add_complements(added);
    const std::span<const Complement> middle = model.complements(1, 2);
    ASSERT_EQ(middle.size(), 2U);
    expect_complement(middle[0], added[1]);
    expect_complement(middle[1], added[2]);
    EXPECT_TRUE(model.complements(3, 0).empty());
    EXPECT_EQ(refusal([&] { model.complements(-1, 1); }),
              "invalid argument: complement -1: unknown");
    EXPECT_EQ(refusal([&] { model.complements(1, 3); }), "invalid argument: complement 3: unknown");
    EXPECT_EQ(refusal([&] { model.complements(4, 0); }), "invalid argument: complement 4: unknown");
    EXPECT_EQ(refusal([&] { model.complements(0, -1); }),
              "invalid argument: argument 'n': -1 is negative");
}

TEST(ComplementTest, Mismatch)
{
    Model model = with_functions(1);
    add(model, pair(0, 0, 0.0, 1.0));
    const std::vector<Index> indices{0};
    failure([&] { model.set_complements(indices, {}); }, biyos::ErrorKind::Internal);
    expect_complement(model.complement(0), pair(0, 0, 0.0, 1.0));
}

TEST(ComplementTest, Moved)
{
    Model model = with_functions(1);
    add(model, pair(0, 0, 0.0, 1.0));
    const Model taken = std::move(model);
    expect_complement(taken.complement(0), pair(0, 0, 0.0, 1.0));
    EXPECT_EQ(model.n_complements(), 0);
    EXPECT_TRUE(model.complements(0, 0).empty());
    EXPECT_EQ(add_refusal(model, pair(0, 0, 0.0, 1.0)),
              "invalid argument: complement 0: function 0 is unknown");
}

TEST(ComplementTest, NoTolerance)
{
    Model model = with_functions(1);
    add(model, pair(0, 0, 0.0, kInfinity));
    expect_tolerance(model.complement_tolerance(0), kNan, kNan);
    EXPECT_EQ(refusal([&] { model.complement_tolerance(1); }),
              "invalid argument: complement 1: unknown");
}

TEST(ComplementTest, OwnTolerance)
{
    Model model = with_complements(3);
    model.set_complement_tolerances(Indices{2, 0}, Reals{1e-6, 0.0}, Reals{1e-3, 2.0});
    expect_tolerance(model.complement_tolerance(0), 0.0, 2.0);
    expect_tolerance(model.complement_tolerance(1), kNan, kNan);
    expect_tolerance(model.complement_tolerance(2), 1e-6, 1e-3);
}

TEST(ComplementTest, PartTolerance)
{
    Model model = with_complements(1);
    model.set_complement_tolerances(Indices{0}, Reals{0.5}, {});
    expect_tolerance(model.complement_tolerance(0), 0.5, kNan);
    model.set_complement_tolerances(Indices{0}, {}, Reals{0.25});
    expect_tolerance(model.complement_tolerance(0), 0.5, 0.25);
    model.set_complement_tolerances(Indices{0}, {}, {});
    expect_tolerance(model.complement_tolerance(0), 0.5, 0.25);
}

TEST(ComplementTest, ToleranceRefusal)
{
    Model model = with_complements(2);
    const std::string refused = "invalid argument: complement 1: ";
    EXPECT_EQ(tolerance_refusal(model, {1}, {kNan}, {}), refused + "absolute tolerance is NaN");
    EXPECT_EQ(tolerance_refusal(model, {1}, {kInfinity}, {}),
              refused + "absolute tolerance is infinity");
    EXPECT_EQ(tolerance_refusal(model, {1}, {}, {-kInfinity}),
              refused + "relative tolerance is minus infinity");
    EXPECT_EQ(tolerance_refusal(model, {1}, {}, {-0.5}),
              refused + "relative tolerance -0.5 is negative");
    EXPECT_EQ(tolerance_refusal(model, {2}, {1.0}, {}), "invalid argument: complement 2: unknown");
    EXPECT_EQ(tolerance_refusal(model, {-1}, {}, {}), "invalid argument: complement -1: unknown");
    expect_tolerance(model.complement_tolerance(1), kNan, kNan);
    model.set_complement_tolerances(Indices{1}, Reals{-0.0}, {});
    expect_tolerance(model.complement_tolerance(1), -0.0, kNan);
}

TEST(ComplementTest, ToleranceAtomic)
{
    Model model = with_complements(2);
    model.set_complement_tolerances(Indices{0}, Reals{1.0}, Reals{2.0});
    EXPECT_NE(tolerance_refusal(model, {1, 0}, {3.0, 4.0}, {5.0, kNan}), "");
    expect_tolerance(model.complement_tolerance(0), 1.0, 2.0);
    expect_tolerance(model.complement_tolerance(1), kNan, kNan);
    const Indices indices{1, 0};
    const Reals short_list{3.0};
    failure([&] { model.set_complement_tolerances(indices, short_list, {}); },
            biyos::ErrorKind::Internal);
    expect_tolerance(model.complement_tolerance(1), kNan, kNan);
}

TEST(ComplementTest, ToleranceLast)
{
    Model model = with_complements(1);
    model.set_complement_tolerances(Indices{0, 0}, Reals{1.0, 2.0}, {});
    expect_tolerance(model.complement_tolerance(0), 2.0, kNan);
}

TEST(ComplementTest, DeletedTolerance)
{
    Model model = with_complements(2);
    model.set_complement_tolerances(Indices{0, 1}, Reals{1.0, 2.0}, Reals{3.0, 4.0});
    const Indices beyond{1, 2};
    EXPECT_EQ(refusal([&] { model.delete_complement_tolerances(beyond); }),
              "invalid argument: complement 2: unknown");
    expect_tolerance(model.complement_tolerance(1), 2.0, 4.0);
    model.delete_complement_tolerances(Indices{1, 1});
    expect_tolerance(model.complement_tolerance(0), 1.0, 3.0);
    expect_tolerance(model.complement_tolerance(1), kNan, kNan);
    model.delete_complement_tolerances(Indices{0, 1});
    expect_tolerance(model.complement_tolerance(0), kNan, kNan);
}

TEST(ComplementTest, ToleranceCopy)
{
    Model model = with_complements(2);
    model.set_complement_tolerances(Indices{1}, Reals{0.5}, {});
    const Model copy = model;
    EXPECT_EQ(biyos::difference(model, copy), "");
    expect_tolerance(copy.complement_tolerance(1), 0.5, kNan);
    Model changed = model;
    changed.set_complement_tolerances(Indices{1}, {}, Reals{0.25});
    EXPECT_EQ(biyos::difference(model, changed),
              "complement 1: relative tolerance nan against 0.25");
    changed.delete_complement_tolerances(Indices{1});
    EXPECT_EQ(biyos::difference(model, changed),
              "complement 1: absolute tolerance 0.5 against nan");
    EXPECT_EQ(biyos::difference(model, copy), "");
}
