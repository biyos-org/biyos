#include "io/native/writers/records.hpp"

#include "core/numbers.hpp"
#include "core/text/text.hpp"
#include "io/native/parts.hpp"
#include "model/model.hpp"

#include <ostream>
#include <span>

namespace biyos::native
{

namespace
{

template <typename Kind> int enum_value(Kind kind) noexcept
{
    return static_cast<int>(kind);
}

void print_variable(std::ostream &output, const Variable &variable)
{
    output << enum_value(variable.kind) << ' ' << text_of(variable.lower) << ' '
           << text_of(variable.upper) << '\n';
}

void print_constraint(std::ostream &output, const Constraint &constraint)
{
    output << constraint.function << ' ' << text_of(constraint.lower) << ' '
           << text_of(constraint.upper) << '\n';
}

void print_objective(std::ostream &output, const Objective &objective)
{
    output << objective.function << ' ' << enum_value(objective.sense) << ' ' << objective.priority
           << ' ' << text_of(objective.weight) << ' ' << text_of(objective.absolute) << ' '
           << text_of(objective.relative) << '\n';
}

void print_indicator(std::ostream &output, const Indicator &indicator)
{
    output << indicator.variable << ' ' << indicator.value << ' ' << indicator.function << ' '
           << text_of(indicator.lower) << ' ' << text_of(indicator.upper) << '\n';
}

void print_complement(std::ostream &output, const Complement &complement)
{
    output << complement.function << ' ' << text_of(complement.lower) << ' '
           << text_of(complement.upper) << ' ' << complement.multiplier << '\n';
}

template <typename Item, typename Print>
void print_items(std::ostream &output, Part part, std::span<const Item> items, Print print)
{
    if (items.empty())
    {
        return;
    }
    print_header(output, part, items.size());
    for (const Item &item : items)
    {
        print(output, item);
    }
}

void print_set(const Model &model, std::ostream &output, Index set)
{
    const std::span<const Member> members = model.members(set, set + 1);
    output << enum_value(model.kind_of_special_ordered_set(set)) << ' ' << members.size();
    for (const Member &member : members)
    {
        output << ' ' << member.variable << ' ' << text_of(member.weight);
    }
    output << '\n';
}

void print_cone(const Model &model, std::ostream &output, Index cone)
{
    const std::span<const Index> functions = model.cone_functions(cone, cone + 1);
    const std::span<const Real> parameters = model.cone_parameters(cone, cone + 1);
    output << enum_value(model.cone_kinds(cone, cone + 1).front()) << ' ' << functions.size();
    for (const Index function : functions)
    {
        output << ' ' << function;
    }
    output << ' ' << parameters.size();
    for (const Real parameter : parameters)
    {
        output << ' ' << text_of(parameter);
    }
    output << '\n';
}

template <typename Print>
void print_each(const Model &model, std::ostream &output, Part part, Index n, Print print)
{
    if (n == 0)
    {
        return;
    }
    print_header(output, part, to_size(n));
    for (Index k = 0; k < n; ++k)
    {
        print(model, output, k);
    }
}

}

void print_variables(const Model &model, std::ostream &output)
{
    const auto variables = model.variables(0, model.n_variables());
    print_items(output, Part::Variables, variables, print_variable);
}

void print_constraints(const Model &model, std::ostream &output)
{
    const auto constraints = model.constraints(0, model.n_constraints());
    print_items(output, Part::Constraints, constraints, print_constraint);
}

void print_objectives(const Model &model, std::ostream &output)
{
    const auto objectives = model.objectives(0, model.n_objectives());
    print_items(output, Part::Objectives, objectives, print_objective);
}

void print_indicators(const Model &model, std::ostream &output)
{
    const auto indicators = model.indicators(0, model.n_indicators());
    print_items(output, Part::Indicators, indicators, print_indicator);
}

void print_sets(const Model &model, std::ostream &output)
{
    print_each(model, output, Part::Sets, model.n_special_ordered_sets(), print_set);
}

void print_cones(const Model &model, std::ostream &output)
{
    print_each(model, output, Part::Cones, model.n_cones(), print_cone);
}

void print_complements(const Model &model, std::ostream &output)
{
    const auto complements = model.complements(0, model.n_complements());
    print_items(output, Part::Complements, complements, print_complement);
}

}
