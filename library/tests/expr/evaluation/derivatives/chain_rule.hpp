#pragma once

#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "expr/graph/graph.hpp"
#include "expr/graph/node.hpp"
#include "expr/graph/points.hpp"
#include "reference.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

namespace biyos::test
{

class ChainRule
{
  public:
    ChainRule(const Graph &graph, std::span<const Real> point)
        : graph_(graph), point_(point), values_(at(graph.size()), 0.0),
          adjoints_(at(graph.size()), 0.0), sizes_(at(graph.size()), 0.0)
    {
        for (Offset node = 0; node < graph.size(); ++node)
        {
            values_[at(node)] = value_of(node);
        }
    }

    void differentiate(Offset root)
    {
        row_.assign(point_.size(), 0.0);
        scale_.assign(point_.size(), 0.0);
        std::ranges::fill(adjoints_, 0.0);
        std::ranges::fill(sizes_, 0.0);
        mark(root);
        adjoints_[at(root)] = 1.0;
        sizes_[at(root)] = 1.0;
        for (Offset node = root; node >= 0; --node)
        {
            if (reached_[at(node)] && carries(node))
            {
                spread(node);
            }
        }
    }

    Real entry(std::size_t variable) const { return row_[variable]; }
    Real scale(std::size_t variable) const { return scale_[variable]; }

  private:
    using Wide = long double;

    static std::size_t at(Offset value) { return static_cast<std::size_t>(value); }

    void mark(Offset root)
    {
        reached_.assign(at(graph_.size()), false);
        reached_[at(root)] = true;
        for (Offset node = root; node >= 0; --node)
        {
            if (!reached_[at(node)])
            {
                continue;
            }
            for (const Offset child : graph_.children(node))
            {
                reached_[at(child)] = true;
            }
        }
    }

    Real child(Offset node, std::size_t k) const { return values_[at(graph_.children(node)[k])]; }

    Real value_of(Offset node) const
    {
        const Real x = graph_.children(node).empty() ? 0.0 : child(node, 0);
        switch (graph_.op(node))
        {
        case Operator::Constant:
            return graph_.constant(node);
        case Operator::Variable:
            return point_[at(graph_.variable(node))];
        case Operator::Sum:
            return sum_of(node);
        case Operator::Product:
            return product_of(node, graph_.children(node).size());
        case Operator::Negation:
            return -x;
        case Operator::Division:
            return x / child(node, 1);
        case Operator::ConstantPower:
            return std::pow(x, graph_.constant(node));
        case Operator::Power:
            return std::pow(x, child(node, 1));
        case Operator::Exp:
            return std::exp(x);
        case Operator::Log:
            return std::log(x);
        case Operator::Sqrt:
            return std::sqrt(x);
        case Operator::Sin:
            return std::sin(x);
        case Operator::Cos:
            return std::cos(x);
        case Operator::Tan:
            return std::tan(x);
        case Operator::Abs:
            return std::fabs(x);
        case Operator::Max:
        case Operator::Min:
            return extreme_of(node);
        case Operator::Asin:
            return std::asin(x);
        case Operator::Acos:
            return std::acos(x);
        case Operator::Atan:
            return std::atan(x);
        case Operator::Sinh:
            return std::sinh(x);
        case Operator::Cosh:
            return std::cosh(x);
        case Operator::Tanh:
            return std::tanh(x);
        case Operator::Asinh:
            return std::asinh(x);
        case Operator::Acosh:
            return std::acosh(x);
        case Operator::Atanh:
            return std::atanh(x);
        case Operator::Entropy:
            return std::fpclassify(x) == FP_ZERO ? 0.0 : x * std::log(x);
        case Operator::SignPower:
            return std::copysign(std::pow(std::fabs(x), graph_.constant(node)), x);
        case Operator::Erf:
            return std::erf(x);
        case Operator::PiecewiseLinear:
            return biyos::value_at(graph_.points().piecewise_linear(graph_.piecewise_linear(node)),
                                   x);
        case Operator::Atan2:
            return std::atan2(x + 0.0, child(node, 1) + 0.0);
        case Operator::LessEqual:
        case Operator::GreaterEqual:
        case Operator::Equal:
        case Operator::Less:
        case Operator::Greater:
        case Operator::NotEqual:
            return comparison(graph_.op(node), x, child(node, 1));
        case Operator::And:
        case Operator::Or:
        case Operator::Not:
            return connective(graph_.op(node), operands_of(node));
        case Operator::If:
            return select(x, child(node, 1), child(node, 2));
        }
        return (std::numeric_limits<Real>::quiet_NaN)();
    }

    Real sum_of(Offset node) const
    {
        const std::span<const Real> weights = graph_.coefficients(node);
        Real total = graph_.constant(node);
        for (std::size_t k = 0; k < weights.size(); ++k)
        {
            total += weights[k] * child(node, k);
        }
        return total;
    }

    Real product_of(Offset node, std::size_t skipped) const
    {
        Real total = graph_.constant(node);
        for (std::size_t k = 0; k < graph_.children(node).size(); ++k)
        {
            if (k != skipped)
            {
                total *= child(node, k);
            }
        }
        return total;
    }

    std::vector<Real> operands_of(Offset node) const
    {
        return children_of(graph_, node, [this](Offset child) { return values_[at(child)]; });
    }

    Real extreme_of(Offset node) const
    {
        const std::vector<Real> operands = operands_of(node);
        if (std::ranges::any_of(operands, [](Real value) { return std::isnan(value); }))
        {
            return (std::numeric_limits<Real>::quiet_NaN)();
        }
        const bool largest = graph_.op(node) == Operator::Max;
        return largest ? (std::ranges::max)(operands) : (std::ranges::min)(operands);
    }

    std::size_t taken(Offset node) const
    {
        if (graph_.op(node) == Operator::If)
        {
            return is_zero(child(node, 0)) ? 2 : 1;
        }
        const auto bits = std::bit_cast<Bits>(values_[at(node)]);
        for (std::size_t k = 0; k < graph_.children(node).size(); ++k)
        {
            if (std::bit_cast<Bits>(child(node, k)) == bits)
            {
                return k;
            }
        }
        return 0;
    }

    static Wide right_root(Wide x) { return 0.5L * std::pow(x, -0.5L); }
    static Wide right_inverse(Wide x) { return std::exp(-std::log(x)); }
    static Wide square(Wide x) { return x * x; }

    static Wide power_slope(Wide x, Wide exponent)
    {
        if (std::fpclassify(exponent) == FP_ZERO)
        {
            return 0.0L;
        }
        const Wide base = std::fpclassify(x) == FP_ZERO ? 0.0L : x;
        return exponent * std::pow(base, exponent - 1.0L);
    }

    static Wide angle_slope(Wide y, Wide x, std::size_t k)
    {
        const Wide radius = x * x + y * y;
        return k == 0 ? x / radius : -y / radius;
    }

    Wide exponent_slope(Offset node, Wide x) const
    {
        const Wide result = values_[at(node)];
        return std::fpclassify(result) == FP_ZERO ? 0.0L : result * std::log(x);
    }

    Wide slope(Offset node, std::size_t k) const
    {
        const Wide x = child(node, 0);
        switch (graph_.op(node))
        {
        case Operator::Constant:
        case Operator::Variable:
            return 0.0L;
        case Operator::Sum:
            return graph_.coefficients(node)[k];
        case Operator::Product:
            return product_of(node, k);
        case Operator::Negation:
            return -1.0L;
        case Operator::Division:
            return k == 0 ? 1.0L / child(node, 1) : -x / square(child(node, 1));
        case Operator::ConstantPower:
            return power_slope(x, graph_.constant(node));
        case Operator::Power:
            return k == 0 ? power_slope(x, child(node, 1)) : exponent_slope(node, x);
        case Operator::Exp:
            return std::exp(x);
        case Operator::Log:
            return right_inverse(x);
        case Operator::Sqrt:
            return right_root(x);
        case Operator::Sin:
            return std::cos(x);
        case Operator::Cos:
            return -std::sin(x);
        case Operator::Tan:
            return 1.0L / square(std::cos(x));
        case Operator::Abs:
            return x < 0.0L ? -1.0L : 1.0L;
        case Operator::Max:
        case Operator::Min:
        case Operator::If:
            return k == taken(node) ? 1.0L : 0.0L;
        case Operator::Asin:
            return 1.0L / std::sqrt(1.0L - x * x);
        case Operator::Acos:
            return -1.0L / std::sqrt(1.0L - x * x);
        case Operator::Atan:
            return 1.0L / (1.0L + x * x);
        case Operator::Sinh:
            return (std::exp(x) + std::exp(-x)) / 2.0L;
        case Operator::Cosh:
            return (std::exp(x) - std::exp(-x)) / 2.0L;
        case Operator::Tanh:
            return 4.0L / square(std::exp(x) + std::exp(-x));
        case Operator::Asinh:
            return 1.0L / std::sqrt(x * x + 1.0L);
        case Operator::Acosh:
            return 1.0L / std::sqrt(x * x - 1.0L);
        case Operator::Atanh:
            return 1.0L / (1.0L - x * x);
        case Operator::Entropy:
            return std::log(x) + 1.0L;
        case Operator::SignPower:
            return graph_.constant(node) * std::pow(std::fabs(x), graph_.constant(node) - 1.0L);
        case Operator::Erf:
            return 2.0L / std::sqrt(std::numbers::pi_v<Wide>) * std::exp(-x * x);
        case Operator::PiecewiseLinear:
            return biyos::slope_at(graph_.points().piecewise_linear(graph_.piecewise_linear(node)),
                                   child(node, 0));
        case Operator::Atan2:
            return angle_slope(x, child(node, 1), k);
        case Operator::LessEqual:
        case Operator::GreaterEqual:
        case Operator::Equal:
        case Operator::Less:
        case Operator::Greater:
        case Operator::NotEqual:
        case Operator::And:
        case Operator::Or:
        case Operator::Not:
            return 0.0L;
        }
        return 0.0L;
    }

    Real partial(Offset node, std::size_t k) const
    {
        if (std::isnan(values_[at(node)]))
        {
            return (std::numeric_limits<Real>::quiet_NaN)();
        }
        return static_cast<Real>(slope(node, k));
    }

    Real bound(Offset node, Real partial) const
    {
        if (std::isnan(partial))
        {
            return 0.0;
        }
        const Real x = child(node, 0);
        const Operator op = graph_.op(node);
        if (op == Operator::Cosh)
        {
            return std::cosh(x);
        }
        if (op == Operator::Entropy)
        {
            return std::fabs(std::log(x)) + 1.0;
        }
        if (op == Operator::Asin || op == Operator::Acos || op == Operator::Acosh ||
            op == Operator::Atanh)
        {
            if (std::fpclassify(partial) == FP_ZERO)
            {
                return 0.0;
            }
            return std::fabs(partial) * (1.0 + (1.0 + x * x) / std::fabs(1.0 - x * x));
        }
        return std::fabs(partial);
    }

    bool carries(Offset node) const
    {
        return std::fpclassify(sizes_[at(node)]) != FP_ZERO ||
               std::fpclassify(adjoints_[at(node)]) != FP_ZERO;
    }

    bool passed(Offset node, std::size_t k) const
    {
        const Operator op = graph_.op(node);
        const bool chosen = op == Operator::Max || op == Operator::Min || op == Operator::If;
        return chosen && !std::isnan(values_[at(node)]) && k != taken(node);
    }

    void spread(Offset node)
    {
        const Real adjoint = adjoints_[at(node)];
        const Real size = sizes_[at(node)];
        if (graph_.op(node) == Operator::Variable)
        {
            row_[at(graph_.variable(node))] += adjoint;
            scale_[at(graph_.variable(node))] += size;
        }
        const std::span<const Offset> children = graph_.children(node);
        for (std::size_t k = 0; k < children.size(); ++k)
        {
            if (passed(node, k))
            {
                continue;
            }
            const Real partial_k = partial(node, k);
            if (std::fpclassify(adjoint) != FP_ZERO)
            {
                adjoints_[at(children[k])] += adjoint * partial_k;
            }
            const Real bound_k = bound(node, partial_k);
            if (std::fpclassify(bound_k) != FP_ZERO)
            {
                sizes_[at(children[k])] += size * bound_k;
            }
        }
    }

    const Graph &graph_;
    std::span<const Real> point_;
    std::vector<Real> values_;
    std::vector<Real> adjoints_;
    std::vector<Real> sizes_;
    std::vector<bool> reached_;
    std::vector<Real> row_;
    std::vector<Real> scale_;
};

}
