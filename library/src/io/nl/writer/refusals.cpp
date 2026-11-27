#include "io/nl/writer/refusals.hpp"

#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "core/text/text.hpp"
#include "expr/graph/node.hpp"
#include "io/nl/source/bounds.hpp"
#include "io/nl/writer/layout.hpp"
#include "io/utils/refuse.hpp"
#include "model/model.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace biyos::nl
{

namespace
{

constexpr std::string_view kFormat = "NL";
constexpr std::string_view kComplement = "complement";

std::string_view kind_text(VariableKind kind) noexcept
{
    switch (kind)
    {
    case VariableKind::Continuous:
    case VariableKind::Binary:
    case VariableKind::Integer:
        break;
    case VariableKind::SemiContinuous:
        return "a semi-continuous variable";
    case VariableKind::SemiInteger:
        return "a semi-integer variable";
    }
    return "";
}

void check_kinds(const Model &model)
{
    for (Index k = 0; k < model.n_variables(); ++k)
    {
        const std::string_view text = kind_text(model.variable(k).kind);
        if (!text.empty())
        {
            refuse_part(kFormat, "variable", k, text);
        }
    }
}

void check_weights(const Model &model)
{
    for (Index k = 0; k < model.n_objectives(); ++k)
    {
        check_weight(kFormat, model, k);
    }
}

bool single_variable(const Model &model, Index function)
{
    const auto terms = model.linear_terms(function, function + 1);
    return terms.size() == 1 && identical(terms.front().coefficient, 1.0) &&
           identical(model.constant(function), 0.0) && !nonlinear(model, function);
}

void check_complement(const Model &model, Index c, std::vector<bool> &paired)
{
    const Complement &complement = model.complement(c);
    if (!single_variable(model, complement.function))
    {
        refuse_part(kFormat, kComplement, c, "a function other than one variable");
    }
    const Index variable = variable_of(model, complement);
    const Variable &bounds = model.variable(variable);
    const std::string label = "variable " + text_of(variable);
    if (!identical(complement.lower, bounds.lower) || !identical(complement.upper, bounds.upper))
    {
        refuse_part(kFormat, kComplement, c, "bounds other than those of " + label);
    }
    if (finite_of(bounds.lower, bounds.upper) == 0)
    {
        refuse_part(kFormat, kComplement, c, "free " + label);
    }
    if (paired[to_size(variable)])
    {
        refuse_part(kFormat, kComplement, c, label + " twice");
    }
    paired[to_size(variable)] = true;
}

void check_complements(const Model &model)
{
    std::vector<bool> paired(to_size(model.n_variables()), false);
    for (Index c = 0; c < model.n_complements(); ++c)
    {
        check_complement(model, c, paired);
    }
}

std::string_view operator_text(Operator op) noexcept
{
    switch (op)
    {
    case Operator::Constant:
    case Operator::Variable:
    case Operator::Sum:
    case Operator::Product:
    case Operator::Negation:
    case Operator::Division:
    case Operator::ConstantPower:
    case Operator::Power:
    case Operator::Exp:
    case Operator::Log:
    case Operator::Sqrt:
    case Operator::Sin:
    case Operator::Cos:
    case Operator::Tan:
    case Operator::Abs:
    case Operator::Max:
    case Operator::Min:
    case Operator::Asin:
    case Operator::Acos:
    case Operator::Atan:
    case Operator::Sinh:
    case Operator::Cosh:
    case Operator::Tanh:
    case Operator::Asinh:
    case Operator::Acosh:
    case Operator::Atanh:
    case Operator::PiecewiseLinear:
        break;
    case Operator::Entropy:
        return "the operator entropy";
    case Operator::SignPower:
        return "the operator signed power";
    case Operator::Erf:
        return "the operator erf";
    case Operator::Atan2:
        return "the operator atan2";
    }
    return "";
}

}

void check_records(const Model &model)
{
    if (model.n_callbacks() > 0)
    {
        refuse_part(kFormat, "callback", 0, "callbacks");
    }
    check_absent(kFormat, model, Family::Cone, "cones");
    check_complements(model);
    check_absent(kFormat, model, Family::Indicator, "indicators");
    check_absent(kFormat, model, Family::SpecialOrderedSet, "special ordered sets");
    check_kinds(model);
    check_weights(model);
}

void check_operator(Operator op, Index function)
{
    const std::string_view text = operator_text(op);
    if (!text.empty())
    {
        refuse_part(kFormat, "function", function, text);
    }
}

}
