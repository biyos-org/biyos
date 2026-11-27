#pragma once

#include "loading.hpp"
#include "sample.hpp"

#include "core/env/env.hpp"
#include "core/env/error.hpp"
#include "core/events/event.hpp"
#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "core/options/lookup.hpp"
#include "io/nl/writer/layout.hpp"
#include "io/nl/writer/usage.hpp"
#include "io/nl/writer/writer.hpp"
#include "model/compare/compare.hpp"
#include "model/evaluation/evaluation.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace biyos::test::nl
{

inline constexpr Real kClose = 1e-12;

inline Parts writable()
{
    Parts parts = sample();
    parts.objectives[0].weight = 1.0;
    parts.indicators = {};
    parts.special_ordered_sets = {.kinds = {}, .starts = {0}, .members = {}};
    parts.cones = {.kinds = {},
                   .starts = {0},
                   .functions = {},
                   .parameter_starts = {0},
                   .parameters = {}};
    parts.complements = {};
    parts.indicator_names = {};
    parts.special_ordered_set_names = {};
    parts.cone_names = {};
    parts.complement_names = {};
    return parts;
}

inline std::string printed(const Model &model)
{
    std::ostringstream output;
    biyos::nl::Plan(model).print(output);
    return output.str();
}

inline Model reread(const Model &model)
{
    return read_sample(printed(model));
}

inline std::string refusal(const Model &model)
{
    try
    {
        static_cast<void>(printed(model));
    }
    catch (const Error &error)
    {
        return error.what();
    }
    return "";
}

inline std::vector<std::string> reports_of(const Model &model)
{
    Env env;
    std::vector<std::string> heard;
    const auto record = [&heard](const Event &event) { heard.push_back(event.text); };
    env.events().add_hook(EventKind::Message, EventLevel::Info, record);
    biyos::nl::Plan(model).report(env.stream(), "t.nl");
    return heard;
}

inline std::vector<Real> values_at(const Model &model, std::span<const Real> x)
{
    Env env;
    Evaluation evaluation(model, env.stream(), env.pool());
    std::vector<Real> f(to_size(model.n_functions()));
    evaluation.values(Lookup(env.options()), x, f);
    return f;
}

inline std::vector<Real> records_at(const Model &model,
                                    std::span<const Index> functions,
                                    std::span<const Real> x)
{
    const std::vector<Real> f = values_at(model, x);
    std::vector<Real> records;
    for (const Index function : functions)
    {
        records.push_back(f[to_size(function)]);
    }
    for (const Objective &objective : model.objectives(0, model.n_objectives()))
    {
        records.push_back(f[to_size(objective.function)]);
    }
    return records;
}

inline bool close(Real found, Real wanted)
{
    if (std::bit_cast<Bits>(found) == std::bit_cast<Bits>(wanted))
    {
        return true;
    }
    const Real scale = 1.0 + (std::max)(std::fabs(found), std::fabs(wanted));
    return std::fabs(found - wanted) <= kClose * scale;
}

inline std::vector<Index> functions_of(std::span<const biyos::nl::Row> rows)
{
    std::vector<Index> functions;
    for (const biyos::nl::Row &row : rows)
    {
        functions.push_back(row.function);
    }
    return functions;
}

inline biyos::nl::Columns columns_in(const Model &model)
{
    using biyos::nl::records_of;
    using biyos::nl::Row;
    using biyos::nl::Usage;
    const std::vector<Row> rows = records_of(model).rows;
    return biyos::nl::columns_of(model, Usage(model, rows));
}

inline void expect_values(const Model &model, const Model &back, std::span<const Real> x)
{
    using biyos::nl::records_of;
    using biyos::nl::Row;
    const std::vector<Row> rows = records_of(model).rows;
    std::vector<Real> moved;
    for (const Index k : columns_in(model).order)
    {
        moved.push_back(x[to_size(k)]);
    }
    std::vector<Index> kept(to_size(back.n_constraints() + back.n_complements()));
    std::iota(kept.begin(), kept.end(), Index{0});
    const std::vector<Real> wanted = records_at(model, functions_of(rows), x);
    const std::vector<Real> found = records_at(back, kept, moved);
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        EXPECT_TRUE(close(found[k], wanted[k]))
            << k << ": " << found[k] << " against " << wanted[k];
    }
}

inline void expect_pair(const Model &back, Index pair, const Complement &wanted, Index column)
{
    const Complement &found = back.complement(pair);
    EXPECT_TRUE(identical(found.lower, wanted.lower)) << "complement " << pair;
    EXPECT_TRUE(identical(found.upper, wanted.upper)) << "complement " << pair;
    const auto terms = back.linear_terms(found.function, found.function + 1);
    ASSERT_EQ(terms.size(), 1U) << "complement " << pair;
    EXPECT_EQ(terms.front().variable, column) << "complement " << pair;
    EXPECT_TRUE(identical(terms.front().coefficient, 1.0)) << "complement " << pair;
}

inline void expect_pairs(const Model &model, const Model &back, std::span<const Index> position)
{
    using biyos::nl::records_of;
    using biyos::nl::Row;
    using biyos::nl::variable_of;
    const std::vector<Row> rows = records_of(model).rows;
    Index pair = 0;
    for (std::size_t m = 0; m < rows.size(); ++m)
    {
        if (rows[m].family != Family::Complement)
        {
            continue;
        }
        ASSERT_LT(pair, back.n_complements());
        const Complement &wanted = model.complement(rows[m].record);
        const Index column = position[to_size(variable_of(model, wanted))];
        expect_pair(back, pair, wanted, column);
        EXPECT_EQ(back.complement(pair).multiplier, to_index(m)) << "complement " << pair;
        ++pair;
    }
    EXPECT_EQ(pair, back.n_complements());
}

inline std::vector<Member> moved_members(const Model &model,
                                         Index set,
                                         std::span<const Index> position)
{
    std::vector<Member> members;
    for (const Member &member : model.members(set, set + 1))
    {
        members.push_back(
            {.variable = position[to_size(member.variable)], .weight = member.weight});
    }
    std::ranges::sort(members, {}, &Member::variable);
    return members;
}

inline void expect_sets(const Model &model, const Model &back, std::span<const Index> position)
{
    ASSERT_EQ(back.n_special_ordered_sets(), model.n_special_ordered_sets());
    for (Index set = 0; set < model.n_special_ordered_sets(); ++set)
    {
        EXPECT_EQ(back.kind_of_special_ordered_set(set), model.kind_of_special_ordered_set(set));
        const std::vector<Member> wanted = moved_members(model, set, position);
        const auto found = back.members(set, set + 1);
        ASSERT_EQ(found.size(), wanted.size()) << "set " << set;
        for (std::size_t k = 0; k < wanted.size(); ++k)
        {
            EXPECT_EQ(found[k].variable, wanted[k].variable) << "set " << set;
            EXPECT_TRUE(identical(found[k].weight, wanted[k].weight)) << "set " << set;
        }
    }
}

inline void expect_kept(const Model &model, std::span<const Real> x)
{
    const Model back = reread(model);
    EXPECT_EQ(printed(back), printed(reread(back)));
    EXPECT_EQ(difference(back, reread(back)), "") << printed(model);
    expect_values(model, back, x);
    const std::vector<Index> position = columns_in(model).position;
    expect_pairs(model, back, position);
    expect_sets(model, back, position);
}

}
