#include "io/draft/support.hpp"

#include "core/numbers.hpp"
#include "io/utils/refuse.hpp"
#include "model/model.hpp"

#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::string_view kFormat = "free MPS";

void check_objectives(const Model &model)
{
    if (model.n_objectives() > 1)
    {
        refuse_part(kFormat, "objective", 1, "a second objective");
    }
    if (model.n_objectives() == 1)
    {
        check_weight(kFormat, model, 0);
    }
}

void check_function(const Model &model, Index function)
{
    if (!model.quadratic_terms(function, function + 1).empty())
    {
        refuse_part(kFormat, "function", function, "quadratic terms");
    }
    if (model.roots(function, function + 1).front() != kNoRoot)
    {
        refuse_part(kFormat, "function", function, "an expression");
    }
}

void check_functions(const Model &model)
{
    for (const Objective &objective : model.objectives(0, model.n_objectives()))
    {
        check_function(model, objective.function);
    }
    for (const Constraint &constraint : model.constraints(0, model.n_constraints()))
    {
        check_function(model, constraint.function);
    }
    for (const Indicator &indicator : model.indicators(0, model.n_indicators()))
    {
        check_function(model, indicator.function);
    }
}

void check_indicators(const Model &model)
{
    std::vector<bool> constrained(to_size(model.n_functions()), false);
    for (const Constraint &constraint : model.constraints(0, model.n_constraints()))
    {
        constrained[to_size(constraint.function)] = true;
    }
    for (Index k = 0; k < model.n_indicators(); ++k)
    {
        if (constrained[to_size(model.indicator(k).function)])
        {
            refuse_part(kFormat, "indicator", k, "an indicator on a function a constraint uses");
        }
    }
}

}

void check_support(const Model &model)
{
    check_objectives(model);
    check_functions(model);
    if (model.n_callbacks() > 0)
    {
        refuse_part(kFormat, "callback", 0, "callbacks");
    }
    check_absent(kFormat, model, Family::Cone, "cones");
    check_absent(kFormat, model, Family::Complement, "complements");
    check_indicators(model);
}

}
