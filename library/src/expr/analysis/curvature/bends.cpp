#include "expr/analysis/curvature/bends.hpp"

#include "core/numbers.hpp"
#include "core/numeric/limits.hpp"
#include "expr/analysis/curvature/shape.hpp"
#include "expr/analysis/intervals/interval.hpp"

#include <limits>

namespace biyos
{

namespace
{
constexpr Domain kUnit{.closure = {-1.0, 1.0}};
constexpr Domain kOpenUnit{.closure = {-1.0, 1.0}, .open_lower = true, .open_upper = true};
constexpr Domain kFromOne{.closure = {1.0, kInfinity}};
constexpr Interval kInverseE{0x1.78b56362cef37p-2, 0x1.78b56362cef38p-2};
static_assert(std::numeric_limits<Real>::digits == 53);

bool touches_lower(const Domain &domain, Interval inside) noexcept
{
    return domain.open_lower && !(inside.lower() > domain.closure.lower());
}

bool touches_upper(const Domain &domain, Interval inside) noexcept
{
    return domain.open_upper && !(inside.upper() < domain.closure.upper());
}

template <typename T> T side(const Split<T> &split, Interval inside) noexcept
{
    if (inside.upper() <= split.at.lower())
    {
        return split.below;
    }
    if (inside.lower() >= split.at.upper())
    {
        return split.above;
    }
    return split.across;
}

}

const Bend kNegation{
    .domain = kReals,
    .curvature = everywhere(Curvature::Affine),
    .monotonicity = everywhere(Monotonicity::Nonincreasing),
};

const Bend kExp{
    .domain = kReals,
    .curvature = everywhere(Curvature::Convex),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

const Bend kLog{
    .domain = kPositives,
    .curvature = everywhere(Curvature::Concave),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

const Bend kSqrt{
    .domain = kNonnegatives,
    .curvature = everywhere(Curvature::Concave),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

const Bend kAbs{
    .domain = kReals,
    .curvature = at_zero(Curvature::Affine, Curvature::Affine, Curvature::Convex),
    .monotonicity =
        at_zero(Monotonicity::Nonincreasing, Monotonicity::Nondecreasing, Monotonicity::Unknown),
};

const Bend kAsin{
    .domain = kUnit,
    .curvature = at_zero(Curvature::Concave, Curvature::Convex, Curvature::Unknown),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

const Bend kAcos{
    .domain = kUnit,
    .curvature = at_zero(Curvature::Convex, Curvature::Concave, Curvature::Unknown),
    .monotonicity = everywhere(Monotonicity::Nonincreasing),
};

const Bend kAtan{
    .domain = kReals,
    .curvature = at_zero(Curvature::Convex, Curvature::Concave, Curvature::Unknown),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

const Bend kSinh{
    .domain = kReals,
    .curvature = at_zero(Curvature::Concave, Curvature::Convex, Curvature::Unknown),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

const Bend kCosh{
    .domain = kReals,
    .curvature = everywhere(Curvature::Convex),
    .monotonicity =
        at_zero(Monotonicity::Nonincreasing, Monotonicity::Nondecreasing, Monotonicity::Unknown),
};

const Bend kTanh{
    .domain = kReals,
    .curvature = at_zero(Curvature::Convex, Curvature::Concave, Curvature::Unknown),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

const Bend kAsinh{
    .domain = kReals,
    .curvature = at_zero(Curvature::Convex, Curvature::Concave, Curvature::Unknown),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

const Bend kAcosh{
    .domain = kFromOne,
    .curvature = everywhere(Curvature::Concave),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

const Bend kAtanh{
    .domain = kOpenUnit,
    .curvature = at_zero(Curvature::Concave, Curvature::Convex, Curvature::Unknown),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

const Bend kEntropy{
    .domain = kNonnegatives,
    .curvature = everywhere(Curvature::Convex),
    .monotonicity =
        {
            .at = kInverseE,
            .below = Monotonicity::Nonincreasing,
            .above = Monotonicity::Nondecreasing,
            .across = Monotonicity::Unknown,
        },
};

const Bend kErf{
    .domain = kReals,
    .curvature = at_zero(Curvature::Convex, Curvature::Concave, Curvature::Unknown),
    .monotonicity = everywhere(Monotonicity::Nondecreasing),
};

Profile bend(const Bend &shape, Interval child) noexcept
{
    const Domain &domain = shape.domain;
    const Interval inside = meet(child, domain.closure);
    const bool lower = touches_lower(domain, inside);
    const bool upper = touches_upper(domain, inside);
    if (inside.is_empty() || (is_point(inside) && (lower || upper)))
    {
        return {};
    }
    Argument argument{
        .monotonicity = side(shape.monotonicity, inside),
        .cut_below = lower || child.lower() < domain.closure.lower(),
        .cut_above = upper || child.upper() > domain.closure.upper(),
    };
    if (is_point(inside))
    {
        argument.monotonicity = Monotonicity::Constant;
        return {.curvature = Curvature::Affine, .argument = argument};
    }
    return {.curvature = side(shape.curvature, inside), .argument = argument};
}

}
