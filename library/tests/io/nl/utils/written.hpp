#pragma once

#include "loading.hpp"
#include "sample.hpp"

#include "core/env/env.hpp"
#include "core/env/error.hpp"
#include "core/events/event.hpp"
#include "core/numbers.hpp"
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
                                    std::span<const Index> constraints,
                                    std::span<const Real> x)
{
    const std::vector<Real> f = values_at(model, x);
    std::vector<Real> records;
    for (const Index k : constraints)
    {
        records.push_back(f[to_size(model.constraint(k).function)]);
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

inline void expect_values(const Model &model, const Model &back, std::span<const Real> x)
{
    const std::vector<Index> constraints = biyos::nl::records_of(model).constraints;
    const biyos::nl::Usage usage(model, constraints);
    std::vector<Real> moved;
    for (const Index k : biyos::nl::columns_of(model, usage).order)
    {
        moved.push_back(x[to_size(k)]);
    }
    std::vector<Index> kept(to_size(back.n_constraints()));
    std::iota(kept.begin(), kept.end(), Index{0});
    const std::vector<Real> wanted = records_at(model, constraints, x);
    const std::vector<Real> found = records_at(back, kept, moved);
    ASSERT_EQ(found.size(), wanted.size());
    for (std::size_t k = 0; k < wanted.size(); ++k)
    {
        EXPECT_TRUE(close(found[k], wanted[k]))
            << k << ": " << found[k] << " against " << wanted[k];
    }
}

inline void expect_kept(const Model &model, std::span<const Real> x)
{
    const Model back = reread(model);
    EXPECT_EQ(difference(back, reread(back)), "") << printed(model);
    expect_values(model, back, x);
}

}
