#include "io/draft/column_shape.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "core/text/text.hpp"
#include "io/draft/draft.hpp"
#include "model/model.hpp"
#include "model/utils/checks.hpp"

#include <cmath>
#include <string>

namespace biyos
{

namespace
{

DraftColumn kinded(VariableKind kind) noexcept
{
    DraftColumn column;
    switch (kind)
    {
    case VariableKind::Continuous:
        break;
    case VariableKind::Binary:
        column.binary = true;
        column.lower = 0.0;
        column.upper = 1.0;
        break;
    case VariableKind::Integer:
        column.marked = true;
        break;
    case VariableKind::SemiContinuous:
        column.semi = true;
        break;
    case VariableKind::SemiInteger:
        column.semi = true;
        column.integer = true;
        break;
    }
    return column;
}

void bound(DraftColumn &column, const Variable &variable)
{
    if (!identical(variable.lower, 0.0))
    {
        column.lower = variable.lower;
    }
    if (column.semi || !identical(upper_of(column), variable.upper))
    {
        column.upper = variable.upper;
    }
    if (negative_upper(column))
    {
        column.lower = variable.lower;
    }
}

bool holds(const DraftColumn &column, const Variable &variable) noexcept
{
    const bool stated = !column.semi || std::isfinite(column.upper.value_or(0.0));
    return stated && kind_of(column) == variable.kind &&
           identical(lower_of(column), variable.lower) &&
           identical(upper_of(column), variable.upper);
}

}

DraftColumn column_shape(Index variable, const Variable &bounds)
{
    DraftColumn column = kinded(bounds.kind);
    if (!column.binary)
    {
        bound(column, bounds);
    }
    if (!holds(column, bounds))
    {
        const std::string range = "[" + text_of(bounds.lower) + ", " + text_of(bounds.upper) + "]";
        const std::string reason = "bounds " + range + " of its kind cannot be written in free MPS";
        throw Error(ErrorKind::Unsupported, item("variable", variable), reason);
    }
    return column;
}

}
