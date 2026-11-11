#include "core/error.hpp"
#include "core/index.hpp"
#include "model/model.hpp"
#include "refusal.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{

using biyos::Entry;
using biyos::Index;
using biyos::Model;
using biyos::Offset;
using biyos::Row;
using biyos::Variable;
using biyos::test::refusal;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

struct Rows
{
    std::vector<Row> bounds;
    std::vector<Offset> starts;
    std::vector<Entry> entries;
};

Model with_variables(std::size_t count)
{
    const Variable free{
        .kind = biyos::VariableKind::Continuous, .lower = -kInfinity, .upper = kInfinity};
    Model model;
    model.add_variables(std::vector<Variable>(count, free));
    return model;
}

void add(Model &model, const Rows &rows)
{
    model.add_rows(rows.bounds, rows.starts, rows.entries);
}

Rows single(Row bounds, std::vector<Entry> entries)
{
    const auto size = static_cast<Offset>(entries.size());
    return Rows{.bounds = {bounds}, .starts = {0, size}, .entries = std::move(entries)};
}

std::string add_refusal(Model &model, const Rows &rows)
{
    return refusal([&] { add(model, rows); });
}

std::string set_refusal(Model &model, Index index, const Row &row)
{
    return refusal([&] { model.set_rows(std::span(&index, 1), std::span(&row, 1)); });
}

void expect_row(const Row &found, const Row &wanted)
{
    EXPECT_DOUBLE_EQ(found.lower, wanted.lower);
    EXPECT_DOUBLE_EQ(found.upper, wanted.upper);
}

void expect_entries(std::span<const Entry> found, const std::vector<Entry> &wanted)
{
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < found.size(); ++k)
    {
        EXPECT_EQ(found[k].column, wanted[k].column) << "entry " << k;
        EXPECT_DOUBLE_EQ(found[k].value, wanted[k].value) << "entry " << k;
    }
}

template <typename Call> std::string failure(Call call, biyos::ErrorKind kind)
{
    try
    {
        call();
    }
    catch (const biyos::Error &error)
    {
        EXPECT_EQ(error.kind(), kind);
        return error.what();
    }
    ADD_FAILURE() << "no error";
    return "";
}

template <typename Call> void expect_internal(Call call)
{
    failure(call, biyos::ErrorKind::Internal);
}

}

TEST(RowTest, Sorted)
{
    Model model = with_variables(4);
    const Rows rows{
        .bounds = {Row{.lower = 0.0, .upper = 1.0}, Row{.lower = -1.0, .upper = 0.0}},
        .starts = {0, 3, 5},
        .entries = {{3, 1.5}, {0, -2.0}, {2, 0.5}, {1, 4.0}, {0, 1.0}},
    };
    add(model, rows);
    ASSERT_EQ(model.row_count(), 2);
    expect_entries(model.entries(0, 1), {{0, -2.0}, {2, 0.5}, {3, 1.5}});
    expect_entries(model.entries(1, 1), {{0, 1.0}, {1, 4.0}});
}

TEST(RowTest, Ranged)
{
    Model model = with_variables(1);
    const Row ranged{.lower = -1.0, .upper = 2.0};
    add(model, single(ranged, {{0, 1.0}}));
    expect_row(model.row(0), ranged);
}

TEST(RowTest, Free)
{
    Model model = with_variables(1);
    const Row free{.lower = -kInfinity, .upper = kInfinity};
    const Row below{.lower = -kInfinity, .upper = 4.0};
    const Row above{.lower = 4.0, .upper = kInfinity};
    add(model, Rows{.bounds = {free, below, above}, .starts = {0, 0, 0, 0}, .entries = {}});
    expect_row(model.row(0), free);
    expect_row(model.row(1), below);
    expect_row(model.row(2), above);
}

TEST(RowTest, Equality)
{
    Model model = with_variables(2);
    const Row equal{.lower = 3.0, .upper = 3.0};
    add(model, single(equal, {{0, 1.0}, {1, 1.0}}));
    expect_row(model.row(0), equal);
}

TEST(RowTest, Reversed)
{
    Model model = with_variables(1);
    const Row reversed{.lower = 5.0, .upper = 1.0};
    add(model, single(reversed, {}));
    expect_row(model.row(0), reversed);
}

TEST(RowTest, Infinities)
{
    Model model = with_variables(1);
    const std::string refused = "invalid argument: row 0: ";
    EXPECT_EQ(add_refusal(model, single(Row{.lower = kInfinity, .upper = kInfinity}, {})),
              refused + "lower bound is infinity");
    EXPECT_EQ(add_refusal(model, single(Row{.lower = -kInfinity, .upper = -kInfinity}, {})),
              refused + "upper bound is minus infinity");
    EXPECT_EQ(model.row_count(), 0);
}

TEST(RowTest, NanBounds)
{
    Model model = with_variables(1);
    const std::string refused = "invalid argument: row 0: ";
    EXPECT_EQ(add_refusal(model, single(Row{.lower = kNan, .upper = 1.0}, {})),
              refused + "lower bound is NaN");
    EXPECT_EQ(add_refusal(model, single(Row{.lower = 0.0, .upper = kNan}, {})),
              refused + "upper bound is NaN");
    EXPECT_EQ(model.row_count(), 0);
}

TEST(RowTest, UnknownColumn)
{
    Model model = with_variables(3);
    const Row row{.lower = 0.0, .upper = 1.0};
    add(model, single(row, {{2, 1.0}}));
    EXPECT_EQ(add_refusal(model, single(row, {{0, 1.0}, {3, 1.0}})),
              "invalid argument: row 1: column 3 is unknown");
    EXPECT_EQ(add_refusal(model, single(row, {{-1, 1.0}})),
              "invalid argument: row 1: column -1 is unknown");
    EXPECT_EQ(model.row_count(), 1);
}

TEST(RowTest, Repeated)
{
    Model model = with_variables(3);
    const Row row{.lower = 0.0, .upper = 1.0};
    EXPECT_EQ(add_refusal(model, single(row, {{2, 1.0}, {0, 1.0}, {2, 3.0}})),
              "invalid argument: row 0: column 2 is repeated");
    EXPECT_EQ(model.row_count(), 0);
}

TEST(RowTest, Coefficients)
{
    Model model = with_variables(2);
    const Row row{.lower = 0.0, .upper = 1.0};
    const std::string refused = "invalid argument: row 0: coefficient of column 1 ";
    EXPECT_EQ(add_refusal(model, single(row, {{0, 1.0}, {1, kNan}})), refused + "is NaN");
    EXPECT_EQ(add_refusal(model, single(row, {{1, kInfinity}})), refused + "is infinity");
    EXPECT_EQ(add_refusal(model, single(row, {{1, -kInfinity}})), refused + "is minus infinity");
    EXPECT_EQ(model.row_count(), 0);
}

TEST(RowTest, Zeros)
{
    Model model = with_variables(3);
    add(model, single(Row{.lower = 0.0, .upper = 1.0}, {{2, 0.0}, {0, 1.0}, {1, -0.0}}));
    expect_entries(model.entries(0, 1), {{0, 1.0}, {1, 0.0}, {2, 0.0}});
}

TEST(RowTest, BadStarts)
{
    Model model = with_variables(2);
    const Row row{.lower = 0.0, .upper = 1.0};
    const Rows late{.bounds = {row}, .starts = {1, 1}, .entries = {{0, 1.0}}};
    EXPECT_EQ(add_refusal(model, late), "invalid argument: argument 'starts': begins at 1, not 0");
    const Rows falling{.bounds = {row, row}, .starts = {0, 2, 1}, .entries = {{0, 1.0}}};
    EXPECT_EQ(add_refusal(model, falling),
              "invalid argument: argument 'starts': decreases from 2 to 1 at entry 2");
    EXPECT_EQ(model.row_count(), 0);
}

TEST(RowTest, Appended)
{
    Model model = with_variables(3);
    const Row row{.lower = 0.0, .upper = 1.0};
    add(model, single(row, {{1, 1.0}, {0, 2.0}}));
    add(model, Rows{.bounds = {row, row}, .starts = {0, 0, 2}, .entries = {{2, 3.0}, {1, 4.0}}});
    ASSERT_EQ(model.row_count(), 3);
    expect_entries(model.entries(0, 3), {{0, 2.0}, {1, 1.0}, {1, 4.0}, {2, 3.0}});
    expect_entries(model.entries(1, 1), {});
    expect_entries(model.entries(2, 1), {{1, 4.0}, {2, 3.0}});
}

TEST(RowTest, Unchanged)
{
    Model model = with_variables(2);
    const Row row{.lower = 0.0, .upper = 1.0};
    add(model, single(row, {{1, 1.0}}));
    const Rows refused{
        .bounds = {row, row}, .starts = {0, 1, 3}, .entries = {{0, 1.0}, {1, 2.0}, {1, 3.0}}};
    EXPECT_NE(add_refusal(model, refused), "");
    const std::vector<Index> indices{0, 1};
    const std::vector<Row> changed{Row{.lower = 2.0, .upper = 3.0}, row};
    EXPECT_NE(refusal([&] { model.set_rows(indices, changed); }), "");
    const std::vector<Index> again{0, 0};
    const std::vector<Row> nan{Row{.lower = 2.0, .upper = 3.0}, Row{.lower = kNan, .upper = 1.0}};
    EXPECT_NE(refusal([&] { model.set_rows(again, nan); }), "");
    ASSERT_EQ(model.row_count(), 1);
    expect_row(model.row(0), row);
    expect_entries(model.entries(0, 1), {{1, 1.0}});
}

TEST(RowTest, LastWins)
{
    Model model = with_variables(1);
    const Row row{.lower = 0.0, .upper = 1.0};
    add(model, Rows{.bounds = {row, row}, .starts = {0, 1, 1}, .entries = {{0, 1.0}}});
    const std::vector<Index> indices{1, 0, 1};
    const std::vector<Row> changed{Row{.lower = 1.0, .upper = 2.0}, Row{.lower = 3.0, .upper = 4.0},
                                   Row{.lower = -kInfinity, .upper = 6.0}};
    model.set_rows(indices, changed);
    expect_row(model.row(0), changed[1]);
    expect_row(model.row(1), changed[2]);
    expect_entries(model.entries(0, 1), {{0, 1.0}});
}

TEST(RowTest, BadIndex)
{
    Model model = with_variables(1);
    add(model, single(Row{.lower = 0.0, .upper = 1.0}, {}));
    EXPECT_EQ(refusal([&] { model.row(-1); }), "invalid argument: row -1: unknown");
    EXPECT_EQ(refusal([&] { model.row(1); }), "invalid argument: row 1: unknown");
    EXPECT_EQ(set_refusal(model, 1, Row{.lower = 0.0, .upper = 1.0}),
              "invalid argument: row 1: unknown");
    EXPECT_EQ(set_refusal(model, 0, Row{.lower = kInfinity, .upper = 1.0}),
              "invalid argument: row 0: lower bound is infinity");
}

TEST(RowTest, ReadRange)
{
    Model model = with_variables(2);
    const std::vector<Row> bounds{Row{.lower = 0.0, .upper = 1.0}, Row{.lower = 2.0, .upper = 3.0},
                                  Row{.lower = 4.0, .upper = 5.0}};
    add(model, Rows{.bounds = bounds,
                    .starts = {0, 1, 3, 4},
                    .entries = {{0, 1.0}, {1, 2.0}, {0, 3.0}, {1, 4.0}}});
    const std::span<const Row> middle = model.rows(1, 2);
    ASSERT_EQ(middle.size(), 2U);
    expect_row(middle[0], bounds[1]);
    expect_row(middle[1], bounds[2]);
    expect_entries(model.entries(1, 2), {{0, 3.0}, {1, 2.0}, {1, 4.0}});
    EXPECT_TRUE(model.rows(3, 0).empty());
    EXPECT_TRUE(model.entries(3, 0).empty());
    EXPECT_EQ(refusal([&] { model.rows(-1, 1); }), "invalid argument: row -1: unknown");
    EXPECT_EQ(refusal([&] { model.entries(1, 3); }), "invalid argument: row 3: unknown");
    EXPECT_EQ(refusal([&] { model.entries(4, 0); }), "invalid argument: row 4: unknown");
    EXPECT_EQ(refusal([&] { model.entries(0, -1); }),
              "invalid argument: argument 'count': -1 is negative");
}

TEST(RowTest, Mismatch)
{
    Model model = with_variables(1);
    const Row row{.lower = 0.0, .upper = 1.0};
    expect_internal([&] { add(model, Rows{.bounds = {row}, .starts = {0}, .entries = {}}); });
    expect_internal([&] { add(model, Rows{.bounds = {row}, .starts = {0, 2}, .entries = {}}); });
    const std::vector<Index> indices{0};
    expect_internal([&] { model.set_rows(indices, {}); });
    EXPECT_EQ(model.row_count(), 0);
}

TEST(RowTest, Room)
{
    Model model = with_variables(1);
    const Offset most = (std::numeric_limits<Offset>::max)();
    const auto call = [&] { model.check_room(1, most); };
    EXPECT_EQ(failure(call, biyos::ErrorKind::NoMemory),
              "out of memory: model: no entry offset left");
    model.check_room(1, 1);
    add(model, single(Row{.lower = 0.0, .upper = 1.0}, {{0, 1.0}}));
    EXPECT_EQ(model.row_count(), 1);
}

TEST(RowTest, Moved)
{
    Model model = with_variables(1);
    const Row row{.lower = 0.0, .upper = 1.0};
    add(model, single(row, {{0, 2.0}}));
    const Model taken = std::move(model);
    expect_entries(taken.entries(0, 1), {{0, 2.0}});
    EXPECT_EQ(model.row_count(), 0);
    EXPECT_TRUE(model.entries(0, 0).empty());
    add(model, single(row, {}));
    expect_entries(model.entries(0, 1), {});
    Model assigned;
    assigned = std::move(model);
    add(model, single(row, {}));
    EXPECT_EQ(model.row_count(), 1);
    EXPECT_EQ(assigned.row_count(), 1);
}
