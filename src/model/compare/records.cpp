#include "model/compare/fields.hpp"

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "model/attributes/records.hpp"
#include "model/expressions/callbacks.hpp"
#include "model/model.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

namespace
{

int kind_of(const Variable &variable)
{
    return static_cast<int>(variable.kind);
}

int sense_of(const Objective &objective)
{
    return static_cast<int>(objective.sense);
}

std::string presence(std::string_view field, const Routine &a, const Routine &b)
{
    if (static_cast<bool>(a) == static_cast<bool>(b))
    {
        return "";
    }
    return std::string(field) + (a ? " given against none" : " none against given");
}

template <typename T> std::span<const T> all_of(const std::vector<T> &items)
{
    return items;
}

std::string value_table(std::string_view noun,
                        std::string_view field,
                        std::span<const Index> a,
                        std::span<const Index> b)
{
    const auto entry = [&](Offset k) { return compared(field, at(a, k), at(b, k)); };
    return table_difference(noun, size_of(a), size_of(b), entry);
}

}

std::string differ(const Variable &a, const Variable &b)
{
    return first_of({
        compared("kind", kind_of(a), kind_of(b)),
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
        compared("sense", sense_of(a), sense_of(b)),
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

std::string differ(Index a, Index b)
{
    return compared("function", a, b);
}

std::string differ(double a, double b)
{
    return compared("value", a, b);
}

std::string differ(const Tolerance &a, const Tolerance &b)
{
    return first_of({
        compared("absolute tolerance", a.absolute, b.absolute),
        compared("relative tolerance", a.relative, b.relative),
    });
}

std::string differ(const Callback &a, const Callback &b)
{
    return first_of({
        value_table("covered function", "index", a.covered, b.covered),
        a.data == b.data ? "" : "data differs",
        presence("value function", a.function, b.function),
        term_table("Jacobian entry", all_of(a.jacobian_pairs), all_of(b.jacobian_pairs)),
        presence("Jacobian function", a.jacobian, b.jacobian),
        term_table("Hessian entry", all_of(a.hessian_pairs), all_of(b.hessian_pairs)),
        presence("Hessian function", a.hessian, b.hessian),
        presence("Hessian product", a.hessian_product, b.hessian_product),
    });
}

}
