#include "io/utils/leftovers.hpp"

#include "core/events/stream.hpp"
#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "core/options/settings.hpp"
#include "io/utils/report.hpp"
#include "model/model.hpp"

#include <cmath>
#include <string>
#include <string_view>

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

bool loose(const Model &model)
{
    for (const Objective &objective : model.objectives(0, model.n_objectives()))
    {
        if (!identical(objective.absolute, 0.0) || !identical(objective.relative, 0.0))
        {
            return true;
        }
    }
    return false;
}

bool ranked(const Model &model)
{
    for (Index k = 0; k < model.n_objectives(); ++k)
    {
        if (model.objective(k).priority != k)
        {
            return true;
        }
    }
    return false;
}

struct Place
{
    Stream stream;
    std::string file;
    std::string_view format;
};

void leave_out(const Place &place, bool left, std::string_view part)
{
    if (left)
    {
        const std::string text = std::string(place.format) + " leaves out " + std::string(part);
        warn(place.stream, place.file, text);
    }
}

}

void warn_leftovers(Stream stream,
                    const Model &model,
                    const Settings &settings,
                    const std::string &file,
                    std::string_view format)
{
    const Place place{.stream = stream, .file = file, .format = format};
    const bool records = held(model, Family::Variable) || held(model, Family::Constraint);
    leave_out(place, records, "the record tolerances");
    leave_out(place, loose(model), "the objective tolerances");
    leave_out(place, ranked(model), "the objective priority");
    leave_out(place, named(model, Family::Indicator), "the indicator names");
    leave_out(place, !settings.empty(), "the model options");
}

}
