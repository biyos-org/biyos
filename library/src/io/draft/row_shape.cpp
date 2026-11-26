#include "io/draft/row_shape.hpp"

#include "core/error.hpp"
#include "core/numbers.hpp"
#include "core/numeric/identical.hpp"
#include "core/numeric/limits.hpp"
#include "core/numeric/zero.hpp"
#include "core/text/text.hpp"
#include "io/draft/draft.hpp"
#include "model/model.hpp"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace biyos
{

namespace
{
[[noreturn]] void refuse_bounds(const std::string &record, Real lower, Real upper)
{
    const std::string bounds = interval_of(lower, upper);
    const std::string reason = "bounds " + bounds + " cannot be written exactly in free MPS";
    throw Error(ErrorKind::Unsupported, record, reason);
}

Real shifted(const std::string &record, Real bound, Real constant)
{
    const Real moved = is_zero(constant) ? bound : bound - constant;
    if (std::isinf(moved) && !std::isinf(bound))
    {
        const std::string reason = "a bound overflows once the constant moves into it";
        throw Error(ErrorKind::Unsupported, record, reason);
    }
    return moved;
}

std::optional<Real> stated(Real value) noexcept
{
    if (identical(value, 0.0))
    {
        return std::nullopt;
    }
    return value;
}

DraftRow plain(RowSense sense, Real rhs) noexcept
{
    return DraftRow{.sense = sense, .rhs = stated(rhs), .range = std::nullopt};
}

DraftRow ranged(RowSense sense, Real rhs, Real range) noexcept
{
    return DraftRow{.sense = sense, .rhs = stated(rhs), .range = range};
}

std::vector<DraftRow> candidates(Real lower, Real upper)
{
    std::vector<DraftRow> rows{
        plain(RowSense::Equal, lower),
        plain(RowSense::Less, upper),
        plain(RowSense::Greater, lower),
    };
    const Real width = upper - lower;
    for (const Real range : {width, std::nextafter(width, 0.0), std::nextafter(width, kInfinity)})
    {
        if (std::isfinite(range))
        {
            rows.push_back(ranged(RowSense::Greater, lower, range));
            rows.push_back(ranged(RowSense::Less, upper, range));
        }
    }
    return rows;
}

bool holds(const DraftRow &row, Real lower, Real upper) noexcept
{
    const Constraint read = constraint_of(0, row);
    const bool finite = std::isfinite(row.rhs.value_or(0.0));
    return finite && identical(read.lower, lower) && identical(read.upper, upper);
}

}

DraftRow free_row(Real constant)
{
    if (is_zero(constant))
    {
        return plain(RowSense::Free, 0.0);
    }
    return plain(RowSense::Free, -constant);
}

DraftRow row_shape(const std::string &record, const Constraint &bounds, Real constant)
{
    if (std::isinf(bounds.lower) && std::isinf(bounds.upper))
    {
        return free_row(constant);
    }
    const Real lower = shifted(record, bounds.lower, constant);
    const Real upper = shifted(record, bounds.upper, constant);
    for (const DraftRow &row : candidates(lower, upper))
    {
        if (holds(row, lower, upper))
        {
            return row;
        }
    }
    refuse_bounds(record, bounds.lower, bounds.upper);
}

}
