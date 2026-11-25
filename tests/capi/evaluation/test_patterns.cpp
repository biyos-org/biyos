#include "biyos/biyos.h"
#include "capi/utils/entry.hpp"
#include "handles.hpp"
#include "mixed.hpp"
#include "model/evaluation/evaluation.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

namespace
{

using biyos::test::build_mixed;
using biyos::test::Env;
using biyos::test::Model;
using Indices = std::vector<BIYOS_index>;
using Counts = std::vector<BIYOS_offset>;

using Count = BIYOS_status (*)(BIYOS_model *, BIYOS_index, BIYOS_index, BIYOS_offset *);
using Read = BIYOS_status (*)(BIYOS_model *,
                              BIYOS_index,
                              BIYOS_index,
                              BIYOS_offset,
                              BIYOS_offset *,
                              BIYOS_index *);
using CountListed = BIYOS_status (*)(BIYOS_model *,
                                     BIYOS_index,
                                     const BIYOS_index *,
                                     BIYOS_offset *);
using ReadListed = BIYOS_status (*)(BIYOS_model *,
                                    BIYOS_index,
                                    const BIYOS_index *,
                                    BIYOS_offset,
                                    BIYOS_offset *,
                                    BIYOS_index *);

struct Records
{
    Counts counts;
    Indices items;

    bool operator==(const Records &) const = default;
};

const Records kJacobian{.counts = {4, 2, 3}, .items = {0, 1, 2, 3, 1, 3, 0, 1, 3}};
const Records kHessian{.counts = {2, 2, 2, 1}, .items = {0, 3, 1, 2, 2, 3, 3}};

Records ranged(const Model &model, Count count, Read read, BIYOS_index beg, BIYOS_index end)
{
    BIYOS_offset nnz = -1;
    EXPECT_EQ(count(model.get(), beg, end, &nnz), BIYOS_OK);
    Records records{.counts = Counts(static_cast<std::size_t>(end - beg), -1),
                    .items = Indices(static_cast<std::size_t>(nnz), -1)};
    EXPECT_EQ(read(model.get(), beg, end, nnz, records.counts.data(), records.items.data()),
              BIYOS_OK);
    return records;
}

Records listed(const Model &model, CountListed count, ReadListed read, const Indices &chosen)
{
    const auto n = static_cast<BIYOS_index>(chosen.size());
    BIYOS_offset nnz = -1;
    EXPECT_EQ(count(model.get(), n, chosen.data(), &nnz), BIYOS_OK);
    Records records{.counts = Counts(chosen.size(), -1),
                    .items = Indices(static_cast<std::size_t>(nnz), -1)};
    EXPECT_EQ(read(model.get(), n, chosen.data(), nnz, records.counts.data(), records.items.data()),
              BIYOS_OK);
    return records;
}

Records jacobian(const Model &model)
{
    return ranged(model, BIYOS_getngpatrg, BIYOS_getgpatrg, 0, 3);
}

Records hessian(const Model &model)
{
    return ranged(model, BIYOS_getnhpatrg, BIYOS_gethpatrg, 0, 4);
}

Records reference_jacobian(const Model &model)
{
    BIYOS_model &handle = *model.get();
    const biyos::Evaluation evaluation(handle.env(), handle.model(), &handle);
    const biyos::Pattern &pattern = evaluation.jacobian_pattern();
    Records records{.counts = {}, .items = {pattern.variables.begin(), pattern.variables.end()}};
    for (std::size_t k = 0; k + 1 < pattern.starts.size(); ++k)
    {
        records.counts.push_back(pattern.starts[k + 1] - pattern.starts[k]);
    }
    return records;
}

Records reference_hessian(const Model &model)
{
    BIYOS_model &handle = *model.get();
    const biyos::Evaluation evaluation(handle.env(), handle.model(), &handle);
    const biyos::Lower &lower = evaluation.hessian_pattern();
    Records records{.counts = {}, .items = {lower.rows.begin(), lower.rows.end()}};
    for (std::size_t k = 0; k + 1 < lower.starts.size(); ++k)
    {
        records.counts.push_back(lower.starts[k + 1] - lower.starts[k]);
    }
    return records;
}

class PatternTest : public testing::Test
{
  protected:
    PatternTest() : model_(env_) { build_mixed(model_.get(), true); }

    Env env_;
    Model model_;
};

}

TEST_F(PatternTest, Jacobian)
{
    EXPECT_EQ(jacobian(model_), kJacobian);
    EXPECT_EQ(jacobian(model_), reference_jacobian(model_));
}

TEST_F(PatternTest, Hessian)
{
    EXPECT_EQ(hessian(model_), kHessian);
    EXPECT_EQ(hessian(model_), reference_hessian(model_));
}

TEST_F(PatternTest, Listed)
{
    const Records rows = listed(model_, BIYOS_getngpatsp, BIYOS_getgpatsp, {2, 0, 2});
    const Records wanted{.counts = {3, 4, 3}, .items = {0, 1, 3, 0, 1, 2, 3, 0, 1, 3}};
    EXPECT_EQ(rows, wanted);
    const Records columns = listed(model_, BIYOS_getnhpatsp, BIYOS_gethpatsp, {3, 1});
    EXPECT_EQ(columns, (Records{.counts = {1, 2}, .items = {3, 1, 2}}));
}

TEST_F(PatternTest, Block)
{
    const Records rows = ranged(model_, BIYOS_getngpatrg, BIYOS_getgpatrg, 1, 3);
    EXPECT_EQ(rows, (Records{.counts = {2, 3}, .items = {1, 3, 0, 1, 3}}));
    const Records none = ranged(model_, BIYOS_getnhpatrg, BIYOS_gethpatrg, 2, 2);
    EXPECT_EQ(none, Records{});
}

TEST_F(PatternTest, Short)
{
    Counts counts(3, -1);
    Indices items(9, -1);
    EXPECT_EQ(BIYOS_getgpatrg(model_.get(), 0, 3, 8, counts.data(), items.data()),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env_.message(), "invalid argument: argument 'cap': holds 8 of the 9 entries");
    EXPECT_EQ(counts, Counts(3, -1));
    EXPECT_EQ(items, Indices(9, -1));
    EXPECT_EQ(BIYOS_gethpatrg(model_.get(), 0, 4, -1, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env_.message(), "invalid argument: argument 'cap': -1 is negative");
}

TEST_F(PatternTest, Nulls)
{
    Counts counts(4, -1);
    EXPECT_EQ(BIYOS_gethpatrg(model_.get(), 0, 4, 7, counts.data(), nullptr), BIYOS_OK);
    EXPECT_EQ(counts, kHessian.counts);
    Indices items(9, -1);
    EXPECT_EQ(BIYOS_getgpatrg(model_.get(), 0, 3, 9, nullptr, items.data()), BIYOS_OK);
    EXPECT_EQ(items, kJacobian.items);
    EXPECT_EQ(BIYOS_getngpatrg(model_.get(), 0, 3, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(env_.message(), "invalid argument: argument 'nnz': is null");
    EXPECT_EQ(BIYOS_getnhpatsp(model_.get(), 1, nullptr, nullptr), BIYOS_ERR_BADARG);
    EXPECT_EQ(BIYOS_getngpatrg(nullptr, 0, 3, nullptr), BIYOS_ERR_BADARG);
}

TEST_F(PatternTest, Unknown)
{
    BIYOS_offset nnz = -1;
    const Indices chosen{0, 3};
    EXPECT_EQ(BIYOS_getngpatsp(model_.get(), 2, chosen.data(), &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(env_.message(), "invalid argument: function 3: unknown");
    const Indices negative{-1};
    EXPECT_EQ(BIYOS_gethpatsp(model_.get(), 1, negative.data(), 0, nullptr, nullptr),
              BIYOS_ERR_BADARG);
    EXPECT_EQ(env_.message(), "invalid argument: variable -1: unknown");
    EXPECT_EQ(BIYOS_getnhpatrg(model_.get(), 0, 5, &nnz), BIYOS_ERR_BADARG);
    EXPECT_EQ(nnz, -1);
    EXPECT_EQ(model_.get()->cached(), nullptr);
}

TEST_F(PatternTest, Rebuilt)
{
    EXPECT_EQ(jacobian(model_), kJacobian);
    const biyos::Evaluation *built = model_.get()->cached();
    EXPECT_EQ(hessian(model_), kHessian);
    EXPECT_EQ(model_.get()->cached(), built);
    const BIYOS_index function = 1;
    const BIYOS_index variable = 0;
    const double coefficient = 1.0;
    ASSERT_EQ(BIYOS_addlintermsp(model_.get(), 1, &function, &variable, &coefficient), BIYOS_OK);
    const Records grown{.counts = {4, 3, 3}, .items = {0, 1, 2, 3, 0, 1, 3, 0, 1, 3}};
    EXPECT_EQ(jacobian(model_), grown);
}
