#include "io/leftovers.hpp"

#include "core/env.hpp"
#include "core/index.hpp"
#include "core/settings.hpp"
#include "io/identical.hpp"
#include "io/report.hpp"
#include "model/model.hpp"

#include <cmath>
#include <string>

namespace biyos
{

namespace
{

bool held(const Model &model, Family family)
{
    for (Index k = 0; k < model.n_of(family); ++k)
    {
        const Override tolerance = model.tolerance_of(family, k);
        if (!std::isnan(tolerance.absolute) || !std::isnan(tolerance.relative))
        {
            return true;
        }
    }
    return false;
}

bool loose(const Objective &objective) noexcept
{
    return !identical(objective.absolute, 0.0) || !identical(objective.relative, 0.0);
}

void leave_out(Env &env, const std::string &file, bool left, const std::string &part)
{
    if (left)
    {
        warn(env, file, "free MPS leaves out " + part);
    }
}

}

void warn_leftovers(Env &env, const Model &model, const Settings &settings, const std::string &file)
{
    const bool lone = model.n_objectives() == 1;
    const bool records = held(model, Family::Variable) || held(model, Family::Constraint);
    leave_out(env, file, records, "the record tolerances");
    leave_out(env, file, lone && loose(model.objective(0)), "the objective tolerances");
    leave_out(env, file, lone && model.objective(0).priority != 0, "the objective priority");
    leave_out(env, file, !settings.empty(), "the model options");
}

}
