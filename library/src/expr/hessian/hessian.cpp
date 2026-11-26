#include "expr/hessian/hessian.hpp"

#include "core/error.hpp"
#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "core/text/text.hpp"
#include "expr/derivatives/value.hpp"
#include "expr/graph/graph.hpp"
#include "expr/hessian/elements.hpp"
#include "expr/hessian/sweep.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::size_t kWidest = std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1);

struct Chunk
{
    std::size_t first;
    std::size_t count;
};

[[noreturn]] void refuse(std::string object, std::string reason)
{
    throw Error(ErrorKind::InvalidArgument, std::move(object), std::move(reason));
}

void check_size(std::string_view object,
                std::size_t given,
                std::size_t wanted,
                std::string_view name)
{
    if (given != wanted)
    {
        refuse(std::string(object),
               "size " + text_of(given) + ", not " + std::string(name) + " " + text_of(wanted));
    }
}

std::span<const Index> variables_of(const Pattern &pattern, Offset e) noexcept
{
    return slice(std::span(pattern.variables), pattern.starts, to_size(e));
}

std::vector<std::size_t> widths_of(std::size_t lanes)
{
    if (lanes > kWidest)
    {
        refuse("lanes", text_of(lanes) + " above " + text_of(kWidest));
    }
    std::vector<std::size_t> widths;
    for (std::size_t width = 1; width < lanes; width *= 2)
    {
        widths.push_back(width);
    }
    widths.push_back(lanes);
    return widths;
}

std::size_t class_of(std::span<const std::size_t> widths, std::size_t size) noexcept
{
    const auto fits = std::ranges::lower_bound(widths, size);
    return fits == widths.end() ? widths.size() - 1
                                : static_cast<std::size_t>(fits - widths.begin());
}

std::size_t offset(std::size_t size, std::size_t column, std::size_t row) noexcept
{
    return column * size - column * (column - 1) / 2 + row - column;
}

void seed(std::span<const Index> variables, Chunk chunk, Pass &pass, Real value) noexcept
{
    for (std::size_t l = 0; l < chunk.count; ++l)
    {
        const auto variable = to_size(variables[chunk.first + l]);
        pass.seeds[variable * pass.lanes + l] = value;
    }
}

void clear(std::span<const Index> variables, Pass &pass) noexcept
{
    for (const Index variable : variables)
    {
        std::ranges::fill(row(std::span(pass.second.gradient), to_size(variable), pass.lanes), 0.0);
    }
}

}

Hessian::Hessian(const Graph &graph,
                 std::span<const Offset> roots,
                 std::span<const Entry> declared,
                 std::size_t lanes,
                 Offset budget)
    : roots_(roots.size()), elements_(elements_of(graph, roots)),
      tape_(graph, elements_.nodes, budget), lower_(lower_of(tape_.pattern(), declared)),
      map_(map_of(lower_, tape_.pattern(), declared))
{
    const std::vector<std::size_t> widths = widths_of(lanes_of(lanes));
    std::vector<bool> used(widths.size(), false);
    used.front() = true;
    for (Offset e = 0; e < tape_.roots(); ++e)
    {
        classes_.push_back(class_of(widths, variables_of(tape_.pattern(), e).size()));
        used[classes_.back()] = true;
    }
    std::vector<std::size_t> places(widths.size(), 0);
    for (std::size_t c = 0; c < widths.size(); ++c)
    {
        if (used[c])
        {
            places[c] = passes_.size();
            passes_.emplace_back(tape_, widths[c]);
        }
    }
    for (std::size_t &c : classes_)
    {
        c = places[c];
    }
}

const Lower &Hessian::pattern() const noexcept
{
    return lower_;
}

void Hessian::widen(Index columns)
{
    const std::size_t wanted = to_size(columns) + 1;
    lower_.starts.resize((std::max)(lower_.starts.size(), wanted), lower_.starts.back());
}

std::span<const Offset> Hessian::declared() const noexcept
{
    return map_.declared;
}

void Hessian::check(std::span<const Real> point, std::span<const Real> weights) const
{
    if (point.size() < to_size(tape_.variables()))
    {
        refuse("point",
               "size " + text_of(point.size()) + " does not cover variable " +
                   text_of(tape_.variables() - 1));
    }
    check_size("weights", weights.size(), roots_, "the root count");
}

void Hessian::add_block(Offset e, Real scale, std::span<Real> out) noexcept
{
    Pass &pass = passes_[classes_[to_size(e)]];
    const std::span<const Index> variables = variables_of(tape_.pattern(), e);
    const std::size_t size = variables.size();
    const std::span<const Offset> blocks =
        std::span(map_.blocks).subspan(to_size(map_.starts[to_size(e)]));
    for (std::size_t first = 0; first < size; first += pass.lanes)
    {
        const Chunk chunk{.first = first, .count = (std::min)(pass.lanes, size - first)};
        seed(variables, chunk, pass, 1.0);
        sweep(tape_, e, pass);
        for (std::size_t l = 0; l < chunk.count; ++l)
        {
            const std::size_t column = first + l;
            for (std::size_t a = column; a < size; ++a)
            {
                const auto variable = to_size(variables[a]);
                out[to_size(blocks[offset(size, column, a)])] +=
                    scale * pass.second.gradient[variable * pass.lanes + l];
            }
        }
        clear(variables, pass);
        seed(variables, chunk, pass, 0.0);
    }
}

void Hessian::add_product(Offset e,
                          Real scale,
                          std::span<const Real> direction,
                          std::span<Real> out) noexcept
{
    Pass &pass = passes_.front();
    const std::span<const Index> variables = variables_of(tape_.pattern(), e);
    for (const Index variable : variables)
    {
        pass.seeds[to_size(variable)] = direction[to_size(variable)];
    }
    sweep(tape_, e, pass);
    for (const Index variable : variables)
    {
        const auto k = to_size(variable);
        out[k] += scale * std::exchange(pass.second.gradient[k], 0.0);
        pass.seeds[k] = 0.0;
    }
}

void Hessian::values(std::span<const Real> point,
                     std::span<const Real> weights,
                     std::span<Real> out)
{
    check(point, weights);
    check_size("out", out.size(), lower_.rows.size(), "the pattern size");
    std::ranges::fill(out, 0.0);
    const std::span<const Real> used = point.first(to_size(tape_.variables()));
    for (Pass &pass : passes_)
    {
        broadcast(used, pass.lanes, pass.points);
    }
    for (Offset e = 0; e < tape_.roots(); ++e)
    {
        const std::size_t k = to_size(e);
        const Real scale = weights[to_size(elements_.roots[k])] * elements_.weights[k];
        if (!is_zero(scale))
        {
            add_block(e, scale, out);
        }
    }
}

void Hessian::product(std::span<const Real> point,
                      std::span<const Real> weights,
                      std::span<const Real> direction,
                      std::span<Real> out)
{
    check(point, weights);
    check_size("direction", direction.size(), point.size(), "the point size");
    check_size("out", out.size(), point.size(), "the point size");
    std::ranges::fill(out, 0.0);
    Pass &pass = passes_.front();
    broadcast(point.first(to_size(tape_.variables())), pass.lanes, pass.points);
    for (Offset e = 0; e < tape_.roots(); ++e)
    {
        const std::size_t k = to_size(e);
        const Real scale = weights[to_size(elements_.roots[k])] * elements_.weights[k];
        if (!is_zero(scale))
        {
            add_product(e, scale, direction, out);
        }
    }
}

}
