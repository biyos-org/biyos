#include "io/draft/support.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/text/text.hpp"
#include "io/identical.hpp"
#include "model/checks.hpp"
#include "model/model.hpp"

#include <string>
#include <string_view>

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
    if (!model.quadratic_terms(function, 1).empty())
    {
        refuse_part("function", function, "quadratic terms");
    }
    if (model.roots(function, 1).front() != kNoRoot)
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
    check_absent(model, Family::SpecialOrderedSet, "sets");
    check_absent(model, Family::Indicator, "indicators");
}

}
