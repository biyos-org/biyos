#include "io/draft/support.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "core/text/text.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

[[noreturn]] void refuse_part(std::string_view noun, Index index, std::string_view part)
{
    const std::string reason = "free MPS cannot hold " + std::string(part);
    throw Error(ErrorKind::Unsupported, item(noun, index), reason);
}

void check_objectives(const Model &model)
{
    if (model.n_objectives() > 1)
    {
        refuse_part("objective", 1, "a second objective");
    }
    if (model.n_objectives() == 1 && !identical(model.objective(0).weight, 1.0))
    {
        const std::string weight = text_of(model.objective(0).weight);
        refuse_part("objective", 0, "a weight other than 1, here " + weight);
    }
}

void check_function(const Model &model, Index function)
{
    if (!model.quadratic_terms(function, function + 1).empty())
    {
        refuse_part("function", function, "quadratic terms");
    }
    if (model.roots(function, function + 1).front() != kNoRoot)
    {
        refuse_part("function", function, "an expression");
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
            refuse_part("indicator", k, "an indicator on a function a constraint uses");
        }
    }
}

void check_absent(const Model &model, Family family, std::string_view part)
{
    if (model.n_of(family) > 0)
    {
        refuse_part(noun_of(family), 0, part);
    }
}

}

void check_support(const Model &model)
{
    check_objectives(model);
    check_functions(model);
    if (model.n_callbacks() > 0)
    {
        refuse_part("callback", 0, "callbacks");
    }
    check_absent(model, Family::Cone, "cones");
    check_absent(model, Family::Complement, "complements");
    check_indicators(model);
}

}
