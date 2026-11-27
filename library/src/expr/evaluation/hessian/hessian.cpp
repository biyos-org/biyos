#include "expr/evaluation/hessian/hessian.hpp"

#include "core/env/error.hpp"
#include "core/env/pool.hpp"
#include "core/memory/resize.hpp"
#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "core/numeric/zero.hpp"
#include "core/text/text.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/evaluation/hessian/sweep.hpp"
#include "expr/graph/graph.hpp"
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
constexpr Offset kGrain = 64;

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

std::vector<Offset> chunks_of(const Pattern &pattern)
{
    std::vector<Offset> chunks{0};
    const auto elements = to_offset(pattern.starts.size() - 1);
    for (Offset e = 0; e < elements; ++e)
    {
        if (pattern.starts[to_size(e) + 1] - pattern.starts[to_size(chunks.back())] >= kGrain)
        {
            chunks.push_back(e + 1);
        }
    }
    if (chunks.back() != elements)
    {
        chunks.push_back(elements);
    }
    return chunks;
}

std::vector<Pass> passes_of(const Tape &tape, std::span<const std::size_t> widths)
{
    std::vector<Pass> passes;
    for (const std::size_t width : widths)
    {
        passes.emplace_back(tape, width);
    }
    return passes;
}

template <typename Place>
void gather(std::span<const Real> parts,
            std::span<const Place> places,
            std::span<Real> out) noexcept
{
    for (std::size_t k = 0; k < places.size(); ++k)
    {
        out[to_size(places[k])] += parts[k];
    }
}

}

Hessian::Hessian(const Graph &graph,
                 std::span<const Offset> roots,
                 std::span<const Entry> declared,
                 std::size_t lanes,
                 Offset budget)
    : roots_(roots.size()), elements_(elements_of(graph, roots)),
      tape_(graph, elements_.nodes, budget), blocks_(dense_of(tape_.pattern())),
      lower_(lower_of(tape_.pattern(), blocks_, declared)),
      map_(map_of(lower_, tape_.pattern(), blocks_, declared))
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
            places[c] = widths_.size();
            widths_.push_back(widths[c]);
        }
    }
    for (std::size_t &c : classes_)
    {
        c = places[c];
    }
    workers_.push_back(passes_of(tape_, widths_));
    chunks_ = chunks_of(tape_.pattern());
    scales_.assign(to_size(tape_.roots()), 0.0);
    parts_.assign((std::max)(map_.blocks.size(), tape_.pattern().variables.size()), 0.0);
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

void Hessian::fit(std::size_t threads)
{
    resize(workers_, threads, [&] { return passes_of(tape_, widths_); });
}

void Hessian::spread(std::span<const Real> point, std::size_t worker, std::size_t passes) noexcept
{
    for (Pass &pass : std::span(workers_[worker]).first(passes))
    {
        repeat(point, pass.lanes, pass.points);
    }
}

void Hessian::weigh(std::span<const Real> weights) noexcept
{
    for (std::size_t k = 0; k < scales_.size(); ++k)
    {
        scales_[k] = weights[to_size(elements_.roots[k])] * elements_.weights[k];
    }
}

void Hessian::write_block(Offset e, Real scale, std::vector<Pass> &passes) noexcept
{
    Pass &pass = passes[classes_[to_size(e)]];
    const std::span<const Index> variables = variables_of(tape_.pattern(), e);
    const std::size_t size = variables.size();
    const std::span<Real> parts = std::span(parts_).subspan(to_size(map_.starts[to_size(e)]));
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
                parts[offset(size, column, a)] =
                    scale * pass.second.gradient[variable * pass.lanes + l];
            }
        }
        clear(variables, pass);
        seed(variables, chunk, pass, 0.0);
    }
}

void Hessian::write_product(Offset e,
                            Real scale,
                            std::span<const Real> direction,
                            Pass &pass) noexcept
{
    const std::span<const Index> variables = variables_of(tape_.pattern(), e);
    const std::span<Real> parts =
        std::span(parts_).subspan(to_size(tape_.pattern().starts[to_size(e)]));
    for (const Index variable : variables)
    {
        pass.seeds[to_size(variable)] = direction[to_size(variable)];
    }
    sweep(tape_, e, pass);
    for (std::size_t k = 0; k < variables.size(); ++k)
    {
        const auto variable = to_size(variables[k]);
        parts[k] = scale * std::exchange(pass.second.gradient[variable], 0.0);
        pass.seeds[variable] = 0.0;
    }
}

void Hessian::fill_blocks(std::size_t chunk, std::size_t worker) noexcept
{
    for (Offset e = chunks_[chunk]; e < chunks_[chunk + 1]; ++e)
    {
        const Real scale = scales_[to_size(e)];
        if (is_zero(scale))
        {
            std::ranges::fill(slice(std::span(parts_), map_.starts, to_size(e)), 0.0);
        }
        else
        {
            write_block(e, scale, workers_[worker]);
        }
    }
}

void Hessian::fill_products(std::size_t chunk,
                            std::size_t worker,
                            std::span<const Real> direction) noexcept
{
    const Pattern &pattern = tape_.pattern();
    for (Offset e = chunks_[chunk]; e < chunks_[chunk + 1]; ++e)
    {
        const Real scale = scales_[to_size(e)];
        if (is_zero(scale))
        {
            std::ranges::fill(slice(std::span(parts_), pattern.starts, to_size(e)), 0.0);
        }
        else
        {
            write_product(e, scale, direction, workers_[worker].front());
        }
    }
}

void Hessian::values(std::span<const Real> point,
                     std::span<const Real> weights,
                     std::span<Real> out,
                     Pool &pool)
{
    check(point, weights);
    check_size("out", out.size(), lower_.rows.size(), "the pattern size");
    const std::span<const Real> used = point.first(to_size(tape_.variables()));
    const std::size_t passes = widths_.size();
    auto copy = [&](auto worker, auto) noexcept { spread(used, worker, passes); };
    pool.run(workers_.size(), workers_.size(), copy);
    weigh(weights);
    auto fill = [&](auto chunk, auto worker) noexcept { fill_blocks(chunk, worker); };
    pool.run(workers_.size(), chunks_.size() - 1, fill);
    std::ranges::fill(out, 0.0);
    gather(std::span<const Real>(parts_), std::span<const Offset>(map_.blocks), out);
}

void Hessian::product(std::span<const Real> point,
                      std::span<const Real> weights,
                      std::span<const Real> direction,
                      std::span<Real> out,
                      Pool &pool)
{
    check(point, weights);
    check_size("direction", direction.size(), point.size(), "the point size");
    check_size("out", out.size(), point.size(), "the point size");
    const std::span<const Real> used = point.first(to_size(tape_.variables()));
    auto copy = [&](auto worker, auto) noexcept { spread(used, worker, 1); };
    pool.run(workers_.size(), workers_.size(), copy);
    weigh(weights);
    auto fill = [&](auto chunk, auto worker) noexcept { fill_products(chunk, worker, direction); };
    pool.run(workers_.size(), chunks_.size() - 1, fill);
    std::ranges::fill(out, 0.0);
    const std::span<const Index> variables = tape_.pattern().variables;
    gather(std::span<const Real>(parts_), variables, out);
}

}
