#include "io/leftovers.hpp"

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "core/options/settings.hpp"
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
        const Tolerance tolerance = model.tolerance_of(family, k);
        if (!std::isnan(tolerance.absolute) || !std::isnan(tolerance.relative))
        {
            return true;
        }
    }
    return false;
}

bool named(const Model &model, Family family)
{
    for (Index k = 0; k < model.n_of(family); ++k)
    {
        if (model.name_of(family, k) != nullptr)
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

void leave_out(Stream stream, const std::string &file, bool left, const std::string &part)
{
    if (left)
    {
        warn(stream, file, "free MPS leaves out " + part);
    }
}

}

void warn_leftovers(Stream stream,
                    const Model &model,
                    const Settings &settings,
                    const std::string &file)
{
    const bool lone = model.n_objectives() == 1;
    const bool records = held(model, Family::Variable) || held(model, Family::Constraint);
    leave_out(stream, file, records, "the record tolerances");
    leave_out(stream, file, lone && loose(model.objective(0)), "the objective tolerances");
    leave_out(stream, file, lone && model.objective(0).priority != 0, "the objective priority");
    leave_out(stream, file, named(model, Family::Indicator), "the indicator names");
    leave_out(stream, file, !settings.empty(), "the model options");
}

}
