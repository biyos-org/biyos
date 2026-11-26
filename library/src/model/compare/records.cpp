#include "model/compare/fields.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "expr/graph/points.hpp"
#include "model/attributes/records.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <span>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

std::string point_difference(const PiecewiseLinear &a, const PiecewiseLinear &b, Offset k)
{
    return first_of({
        compared("x", a.x[to_size(k)], b.x[to_size(k)]),
        compared("y", a.y[to_size(k)], b.y[to_size(k)]),
    });
}

std::string presence(std::string_view field, const Routine &a, const Routine &b)
{
    if (static_cast<bool>(a) == static_cast<bool>(b))
    {
        return "";
    }
    return std::string(field) + (a ? " given against none" : " none against given");
}

std::string density(bool a, bool b)
{
    if (a == b)
    {
        return "";
    }
    return a ? "dense against listed" : "listed against dense";
}

}

std::string differ(const Variable &a, const Variable &b)
{
    return first_of({
        compared("kind", a.kind, b.kind),
        compared("lower bound", a.lower, b.lower),
        compared("upper bound", a.upper, b.upper),
    });
}

std::string differ(const LinearTerm &a, const LinearTerm &b)
{
    return first_of({
        compared("variable", a.variable, b.variable),
        compared("coefficient", a.coefficient, b.coefficient),
    });
}

std::string differ(const QuadraticTerm &a, const QuadraticTerm &b)
{
    return first_of({
        compared("first variable", a.row, b.row),
        compared("second variable", a.column, b.column),
        compared("coefficient", a.coefficient, b.coefficient),
    });
}

std::string differ(const IndexPair &a, const IndexPair &b)
{
    return first_of({
        compared("row", a.row, b.row),
        compared("column", a.column, b.column),
    });
}

std::string differ(const Constraint &a, const Constraint &b)
{
    return first_of({
        compared("function", a.function, b.function),
        compared("lower bound", a.lower, b.lower),
        compared("upper bound", a.upper, b.upper),
    });
}

std::string differ(const Objective &a, const Objective &b)
{
    return first_of({
        compared("function", a.function, b.function),
        compared("sense", a.sense, b.sense),
        compared("priority", a.priority, b.priority),
        compared("weight", a.weight, b.weight),
        compared("absolute tolerance", a.absolute, b.absolute),
        compared("relative tolerance", a.relative, b.relative),
    });
}

std::string differ(const Indicator &a, const Indicator &b)
{
    return first_of({
        compared("variable", a.variable, b.variable),
        compared("value", a.value, b.value),
        compared("function", a.function, b.function),
        compared("lower bound", a.lower, b.lower),
        compared("upper bound", a.upper, b.upper),
    });
}

std::string differ(const Complement &a, const Complement &b)
{
    return first_of({
        compared("function", a.function, b.function),
        compared("lower bound", a.lower, b.lower),
        compared("upper bound", a.upper, b.upper),
        compared("multiplier", a.multiplier, b.multiplier),
    });
}

std::string differ(const Member &a, const Member &b)
{
    return first_of({
        compared("variable", a.variable, b.variable),
        compared("weight", a.weight, b.weight),
    });
}

std::string differ(const Tolerance &a, const Tolerance &b)
{
    return first_of({
        compared("absolute tolerance", a.absolute, b.absolute),
        compared("relative tolerance", a.relative, b.relative),
    });
}

std::string differ(const PiecewiseLinear &a, const PiecewiseLinear &b)
{
    const auto point = [&](Offset k) { return point_difference(a, b, k); };
    return table_difference("point", to_offset(a.x.size()), to_offset(b.x.size()), point);
}

std::string differ(const Callback &a, const Callback &b)
{
    return first_of({
        value_table("covered function", "index", std::span(a.covered), std::span(b.covered)),
        density(a.dense, b.dense),
        value_table("listed variable", "index", std::span(a.variables), std::span(b.variables)),
        a.data == b.data ? "" : "data differs",
        presence("value function", a.function, b.function),
        term_table("Jacobian entry", std::span(a.jacobian_pairs), std::span(b.jacobian_pairs)),
        presence("Jacobian function", a.jacobian, b.jacobian),
        term_table("Hessian entry", std::span(a.hessian_pairs), std::span(b.hessian_pairs)),
        presence("Hessian function", a.hessian, b.hessian),
        presence("Hessian product", a.hessian_product, b.hessian_product),
    });
}

}
