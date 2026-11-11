#include "expr/elementary.hpp"

#include "expr/interval.hpp"
#include "expr/rounding.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace biyos
{

namespace
{

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kHalfPiUp = 0x1.921fb54442d19p+0;
constexpr double kPiUp = 0x1.921fb54442d19p+1;
constexpr double kInverseE = 0x1.78b56362cef38p-2;

constexpr Interval kReals = Interval::entire();
constexpr Interval kNonnegative{0.0, kInfinity};
constexpr Interval kAboveOne{1.0, kInfinity};
constexpr Interval kUnit{-1.0, 1.0};
constexpr Interval kQuarterTurn{-kHalfPiUp, kHalfPiUp};
constexpr Interval kHalfTurn{0.0, kPiUp};

struct Monotone
{
    double (*value)(double);
    Interval domain;
    Interval image;
    std::optional<double> root;
    int steps;
    bool poles;
    bool rising;
};

constexpr Monotone kExp{
    .value = [](double x) { return std::exp(x); },
    .domain = kReals,
    .image = kNonnegative,
    .root = std::nullopt,
    .steps = kLibmSteps,
    .poles = false,
    .rising = true,
};

constexpr Monotone kLog{
    .value = [](double x) { return std::log(x); },
    .domain = kNonnegative,
    .image = kReals,
    .root = 1.0,
    .steps = kLibmSteps,
    .poles = true,
    .rising = true,
};

constexpr Monotone kSqrt{
    .value = [](double x) { return std::sqrt(x); },
    .domain = kNonnegative,
    .image = kNonnegative,
    .root = 0.0,
    .steps = kRoundedSteps,
    .poles = false,
    .rising = true,
};

constexpr Monotone kAsin{
    .value = [](double x) { return std::asin(x); },
    .domain = kUnit,
    .image = kQuarterTurn,
    .root = 0.0,
    .steps = kLibmSteps,
    .poles = false,
    .rising = true,
};

constexpr Monotone kAcos{
    .value = [](double x) { return std::acos(x); },
    .domain = kUnit,
    .image = kHalfTurn,
    .root = 1.0,
    .steps = kLibmSteps,
    .poles = false,
    .rising = false,
};

constexpr Monotone kAtan{
    .value = [](double x) { return std::atan(x); },
    .domain = kReals,
    .image = kQuarterTurn,
    .root = 0.0,
    .steps = kLibmSteps,
    .poles = false,
    .rising = true,
};

constexpr Monotone kSinh{
    .value = [](double x) { return std::sinh(x); },
    .domain = kReals,
    .image = kReals,
    .root = 0.0,
    .steps = kLibmSteps,
    .poles = false,
    .rising = true,
};

constexpr Monotone kCosh{
    .value = [](double x) { return std::cosh(x); },
    .domain = kNonnegative,
    .image = kAboveOne,
    .root = std::nullopt,
    .steps = kLibmSteps,
    .poles = false,
    .rising = true,
};

constexpr Monotone kTanh{
    .value = [](double x) { return std::tanh(x); },
    .domain = kReals,
    .image = kUnit,
    .root = 0.0,
    .steps = kLibmSteps,
    .poles = false,
    .rising = true,
};

constexpr Monotone kAsinh{
    .value = [](double x) { return std::asinh(x); },
    .domain = kReals,
    .image = kReals,
    .root = 0.0,
    .steps = kLibmSteps,
    .poles = false,
    .rising = true,
};

constexpr Monotone kAcosh{
    .value = [](double x) { return std::acosh(x); },
    .domain = kAboveOne,
    .image = kNonnegative,
    .root = 1.0,
    .steps = kLibmSteps,
    .poles = false,
    .rising = true,
};

constexpr Monotone kAtanh{
    .value = [](double x) { return std::atanh(x); },
    .domain = kUnit,
    .image = kReals,
    .root = 0.0,
    .steps = kLibmSteps,
    .poles = true,
    .rising = true,
};

constexpr Monotone kErf{
    .value = [](double x) { return std::erf(x); },
    .domain = kReals,
    .image = kUnit,
    .root = 0.0,
    .steps = kLibmSteps,
    .poles = false,
    .rising = true,
};

Interval restrict(const Monotone &f, Interval value) noexcept
{
    const Interval inside = meet(value, f.domain);
    if (inside.is_empty() || !f.poles)
    {
        return inside;
    }
    const bool open = inside.upper() > f.domain.lower() && inside.lower() < f.domain.upper();
    return open ? inside : Interval::empty();
}

Interval sign_of(const Monotone &f, Interval inside) noexcept
{
    if (!f.root.has_value())
    {
        return Interval::entire();
    }
    return f.rising ? signs(inside, *f.root) : signs(-inside, -*f.root);
}

Interval monotone(const Monotone &f, Interval value) noexcept
{
    const Interval inside = restrict(f, value);
    if (inside.is_empty())
    {
        return inside;
    }
    const double least = f.value(f.rising ? inside.lower() : inside.upper());
    const double most = f.value(f.rising ? inside.upper() : inside.lower());
    const Interval values(shifted(least, -f.steps), shifted(most, f.steps));
    return meet(meet(values, f.image), sign_of(f, inside));
}

double entropy_value(double value) noexcept
{
    return value > 0.0 ? value * std::log(value) : 0.0;
}

double least_entropy(Interval inside) noexcept
{
    if (inside.lower() > kInverseE)
    {
        return entropy_value(inside.lower());
    }
    if (inside.upper() < kInverseE)
    {
        return entropy_value(inside.upper());
    }
    return entropy_value(kInverseE);
}

}

Interval exp(Interval value) noexcept
{
    return monotone(kExp, value);
}

Interval log(Interval value) noexcept
{
    return monotone(kLog, value);
}

Interval sqrt(Interval value) noexcept
{
    return monotone(kSqrt, value);
}

Interval asin(Interval value) noexcept
{
    return monotone(kAsin, value);
}

Interval acos(Interval value) noexcept
{
    return monotone(kAcos, value);
}

Interval atan(Interval value) noexcept
{
    return monotone(kAtan, value);
}

Interval sinh(Interval value) noexcept
{
    return monotone(kSinh, value);
}

Interval cosh(Interval value) noexcept
{
    return monotone(kCosh, abs(value));
}

Interval tanh(Interval value) noexcept
{
    return monotone(kTanh, value);
}

Interval asinh(Interval value) noexcept
{
    return monotone(kAsinh, value);
}

Interval acosh(Interval value) noexcept
{
    return monotone(kAcosh, value);
}

Interval atanh(Interval value) noexcept
{
    return monotone(kAtanh, value);
}

Interval erf(Interval value) noexcept
{
    return monotone(kErf, value);
}

Interval entropy(Interval value) noexcept
{
    const Interval inside = meet(value, kNonnegative);
    if (inside.is_empty())
    {
        return inside;
    }
    const double left = entropy_value(inside.lower());
    const double right = entropy_value(inside.upper());
    const Interval values(widen_down(least_entropy(inside)), widen_up((std::max)(left, right)));
    return meet(values, signs(inside, 1.0));
}

}
