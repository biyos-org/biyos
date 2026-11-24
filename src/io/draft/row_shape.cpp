#include "io/draft/row_shape.hpp"

#include "core/error.hpp"
#include "core/index.hpp"
#include "core/numeric/limits.hpp"
#include "core/numeric/zero.hpp"
#include "core/text/text.hpp"
#include "io/draft/draft.hpp"
#include "io/identical.hpp"
#include "model/checks.hpp"
#include "model/model.hpp"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace biyos
{

namespace
{
[[noreturn]] void refuse_bounds(Index constraint, double lower, double upper)
{
    const std::string bounds = "[" + text_of(lower) + ", " + text_of(upper) + "]";
    const std::string reason = "bounds " + bounds + " cannot be written exactly in free MPS";
    throw Error(ErrorKind::Unsupported, item("constraint", constraint), reason);
}

double shifted(Index constraint, double bound, double constant)
{
    const double moved = is_zero(constant) ? bound : bound - constant;
    if (std::isinf(moved) && !std::isinf(bound))
    {
        const std::string reason = "a bound overflows once the constant moves into it";
        throw Error(ErrorKind::Unsupported, item("constraint", constraint), reason);
    }
    return moved;
}

std::optional<double> stated(double value) noexcept
{
    if (identical(value, 0.0))
    {
        return std::nullopt;
    }
    return value;
}

DraftRow plain(RowSense sense, double rhs) noexcept
{
    return DraftRow{.sense = sense, .rhs = stated(rhs), .range = std::nullopt};
}

DraftRow ranged(RowSense sense, double rhs, double range) noexcept
{
    return DraftRow{.sense = sense, .rhs = stated(rhs), .range = range};
}

std::vector<DraftRow> candidates(double lower, double upper)
{
    std::vector<DraftRow> rows{
        plain(RowSense::Equal, lower),
        plain(RowSense::Less, upper),
        plain(RowSense::Greater, lower),
    };
    const double width = upper - lower;
    for (const double range : {width, std::nextafter(width, 0.0), std::nextafter(width, kInfinity)})
    {
        if (std::isfinite(range))
        {
            rows.push_back(ranged(RowSense::Greater, lower, range));
            rows.push_back(ranged(RowSense::Less, upper, range));
        }
    }
    return rows;
}

bool holds(const DraftRow &row, double lower, double upper) noexcept
{
    const Constraint read = constraint_of(0, row);
    const bool finite = std::isfinite(row.rhs.value_or(0.0));
    return finite && identical(read.lower, lower) && identical(read.upper, upper);
}

}

DraftRow free_row(double constant)
{
    if (is_zero(constant))
    {
        return plain(RowSense::Free, 0.0);
    }
    return plain(RowSense::Free, -constant);
}

DraftRow row_shape(Index constraint, const Constraint &bounds, double constant)
{
    if (std::isinf(bounds.lower) && std::isinf(bounds.upper))
    {
        return free_row(constant);
    }
    const double lower = shifted(constraint, bounds.lower, constant);
    const double upper = shifted(constraint, bounds.upper, constant);
    for (const DraftRow &row : candidates(lower, upper))
    {
        if (holds(row, lower, upper))
        {
            return row;
        }
    }
    refuse_bounds(constraint, bounds.lower, bounds.upper);
}

}
