#include "expr/evaluation/hessian/hessian.hpp"

#include "core/env/error.hpp"
#include "core/env/pool.hpp"
#include "core/memory/resize.hpp"
#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "core/numeric/tolerances.hpp"
#include "core/numeric/zero.hpp"
#include "core/text/text.hpp"
#include "core/timing/timer.hpp"
#include "expr/evaluation/derivatives/value.hpp"
#include "expr/evaluation/hessian/coloring.hpp"
#include "expr/evaluation/hessian/elements.hpp"
#include "expr/evaluation/hessian/pattern.hpp"
#include "expr/evaluation/hessian/sweep.hpp"
#include "expr/graph/graph.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/frame.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
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
constexpr Offset kDense = -1;

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

void seed(std::span<const Index> variables, Chunk chunk, Pass &pass, Real value) noexcept
{
    for (std::size_t l = 0; l < chunk.count; ++l)
    {
        const auto variable = to_size(variables[chunk.first + l]);
        pass.seeds[variable * pass.lanes + l] = value;
    }
}

void paint(std::span<const Index> variables,
           std::span<const Index> colors,
           Chunk window,
           Pass &pass,
           Real value) noexcept
{
    for (std::size_t k = 0; k < variables.size(); ++k)
    {
        const auto color = to_size(colors[k]);
        if (color >= window.first && color < window.first + window.count)
        {
            pass.seeds[to_size(variables[k]) * pass.lanes + color - window.first] = value;
        }
    }
}

void clear(std::span<const Index> variables, Pass &pass) noexcept
{
    for (const Index variable : variables)
    {
        std::ranges::fill(row(std::span(pass.second.gradient), to_size(variable), pass.lanes), 0.0);
    }
}

bool large(std::size_t size) noexcept
{
    return size > kLanes;
}

bool any_large(const Pattern &pattern) noexcept
{
    for (std::size_t e = 0; e + 1 < pattern.starts.size(); ++e)
    {
        if (large(to_size(pattern.starts[e + 1] - pattern.starts[e])))
        {
            return true;
        }
    }
    return false;
}

void add_exact(Pusher &pusher, Offset e, Blocks &blocks)
{
    std::ranges::copy(pusher.shape(e).entries, std::back_inserter(blocks.entries));
    blocks.starts.push_back(to_offset(blocks.entries.size()));
}

Blocks blocks_of(const Tape &tape, Timing timing)
{
    const Pattern &pattern = tape.pattern();
    if (!any_large(pattern))
    {
        return dense_of(pattern);
    }
    const Timer timer(timing.stream, timing.tree, "expr", "pattern");
    Pusher pusher(tape);
    Blocks blocks{.starts = {0}, .entries = {}};
    for (Offset e = 0; e < tape.roots(); ++e)
    {
        const std::size_t size = variables_of(pattern, e).size();
        if (large(size))
        {
            add_exact(pusher, e, blocks);
        }
        else
        {
            add_dense(size, blocks);
        }
    }
    return blocks;
}

Offset length_of(std::span<const Run> runs) noexcept
{
    Offset length = 0;
    for (const Run run : runs)
    {
        length += run.end - run.begin;
    }
    return length;
}

Offset length_of(const Tape &tape, Offset element)
{
    const std::span<const Block> prepass = tape.blocks().first(to_size(tape.prepass()));
    Offset length = length_of(tape.runs(element));
    for (const Range range : tape.route(element))
    {
        for (Offset position = range.begin; position < range.end; ++position)
        {
            const Block &block = block_of(prepass, position);
            const std::size_t output = to_size(position - block.begin);
            length += length_of(slice(std::span(block.runs), block.starts, output));
        }
    }
    return length;
}

bool cheap(const Tape &tape, Offset element, std::span<const Entry> entries, std::size_t size)
{
    const auto sweeps = to_offset((size + kLanes - 1) / kLanes);
    return star_work(entries, size) <= sweeps * length_of(tape, element);
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
                 Timing timing,
                 std::size_t lanes,
                 Offset budget)
    : roots_(roots.size()), elements_(elements_of(graph, roots)),
      tape_(graph, elements_.nodes, budget), blocks_(blocks_of(tape_, timing)),
      lower_(lower_of(tape_.pattern(), blocks_, declared)),
      map_(map_of(lower_, tape_.pattern(), blocks_, declared))
{
    color(timing);
    const std::vector<std::size_t> widths = widths_of(lanes_of(lanes));
    std::vector<bool> used(widths.size(), false);
    used.front() = true;
    for (Offset e = 0; e < tape_.roots(); ++e)
    {
        classes_.push_back(class_of(widths, directions(e)));
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

void Hessian::color(Timing timing)
{
    places_.assign(to_size(tape_.roots()), kDense);
    if (!any_large(tape_.pattern()))
    {
        return;
    }
    const Timer timer(timing.stream, timing.tree, "expr", "coloring");
    for (Offset e = 0; e < tape_.roots(); ++e)
    {
        const std::size_t size = variables_of(tape_.pattern(), e).size();
        const std::span<const Entry> entries = entries_of(blocks_, e);
        if (large(size) && cheap(tape_, e, entries, size))
        {
            places_[to_size(e)] = to_offset(colorings_.size());
            colorings_.push_back(star_coloring(entries, size));
        }
    }
}

std::size_t Hessian::directions(Offset e) const noexcept
{
    const Offset place = places_[to_size(e)];
    return place == kDense ? variables_of(tape_.pattern(), e).size()
                           : to_size(colorings_[to_size(place)].directions);
}

bool Hessian::colored(Offset element) const noexcept
{
    return places_[to_size(element)] != kDense;
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

void Hessian::weigh(std::span<const Real> weights) noexcept
{
    for (std::size_t k = 0; k < scales_.size(); ++k)
    {
        scales_[k] = weights[to_size(elements_.roots[k])] * elements_.weights[k];
    }
}

void Hessian::write_block(Offset e,
                          Real scale,
                          std::span<const Real> point,
                          const Tolerances &tolerances,
                          std::vector<Pass> &passes) noexcept
{
    Pass &pass = passes[classes_[to_size(e)]];
    const std::span<const Index> variables = variables_of(tape_.pattern(), e);
    const std::span<const Entry> entries = entries_of(blocks_, e);
    const std::span<Real> parts = std::span(parts_).subspan(to_size(map_.starts[to_size(e)]));
    std::size_t k = 0;
    for (std::size_t first = 0; first < variables.size(); first += pass.lanes)
    {
        const Chunk chunk{.first = first,
                          .count = (std::min)(pass.lanes, variables.size() - first)};
        seed(variables, chunk, pass, 1.0);
        sweep(tape_, e, point, tolerances, pass);
        for (; k < entries.size() && to_size(entries[k].column) < first + chunk.count; ++k)
        {
            const auto variable = to_size(variables[to_size(entries[k].row)]);
            const std::size_t lane = to_size(entries[k].column) - first;
            parts[k] = scale * pass.second.gradient[variable * pass.lanes + lane];
        }
        clear(variables, pass);
        seed(variables, chunk, pass, 0.0);
    }
}

void Hessian::write_colored(Offset e,
                            Real scale,
                            std::span<const Real> point,
                            const Tolerances &tolerances,
                            std::vector<Pass> &passes) noexcept
{
    Pass &pass = passes[classes_[to_size(e)]];
    const Coloring &coloring = colorings_[to_size(places_[to_size(e)])];
    const std::span<const Index> variables = variables_of(tape_.pattern(), e);
    const std::span<Real> parts = std::span(parts_).subspan(to_size(map_.starts[to_size(e)]));
    const auto count = to_size(coloring.directions);
    auto read = coloring.reads.begin();
    for (std::size_t first = 0; first < count; first += pass.lanes)
    {
        const Chunk window{.first = first, .count = (std::min)(pass.lanes, count - first)};
        paint(variables, coloring.colors, window, pass, 1.0);
        sweep(tape_, e, point, tolerances, pass);
        for (; read != coloring.reads.end() && to_size(read->color) < first + window.count; ++read)
        {
            const auto variable = to_size(variables[to_size(read->row)]);
            const std::size_t lane = to_size(read->color) - first;
            parts[to_size(read->part)] = scale * pass.second.gradient[variable * pass.lanes + lane];
        }
        clear(variables, pass);
        paint(variables, coloring.colors, window, pass, 0.0);
    }
}

void Hessian::write_product(Offset e,
                            Real scale,
                            std::span<const Real> point,
                            std::span<const Real> direction,
                            const Tolerances &tolerances,
                            Pass &pass) noexcept
{
    const std::span<const Index> variables = variables_of(tape_.pattern(), e);
    const std::span<Real> parts =
        std::span(parts_).subspan(to_size(tape_.pattern().starts[to_size(e)]));
    for (const Index variable : variables)
    {
        pass.seeds[to_size(variable)] = direction[to_size(variable)];
    }
    sweep(tape_, e, point, tolerances, pass);
    for (std::size_t k = 0; k < variables.size(); ++k)
    {
        const auto variable = to_size(variables[k]);
        parts[k] = scale * std::exchange(pass.second.gradient[variable], 0.0);
        pass.seeds[variable] = 0.0;
    }
}

void Hessian::fill_blocks(std::size_t chunk,
                          std::size_t worker,
                          std::span<const Real> point,
                          const Tolerances &tolerances) noexcept
{
    for (Offset e = chunks_[chunk]; e < chunks_[chunk + 1]; ++e)
    {
        const Real scale = scales_[to_size(e)];
        if (is_zero(scale))
        {
            std::ranges::fill(slice(std::span(parts_), map_.starts, to_size(e)), 0.0);
        }
        else if (colored(e))
        {
            write_colored(e, scale, point, tolerances, workers_[worker]);
        }
        else
        {
            write_block(e, scale, point, tolerances, workers_[worker]);
        }
    }
}

struct Hessian::Products
{
    Hessian &hessian;
    std::span<const Real> point;
    std::span<const Real> direction;
    const Tolerances &tolerances;

    void operator()(std::size_t chunk, std::size_t worker) const noexcept
    {
        hessian.fill_products(chunk, worker, point, direction, tolerances);
    }
};

void Hessian::fill_products(std::size_t chunk,
                            std::size_t worker,
                            std::span<const Real> point,
                            std::span<const Real> direction,
                            const Tolerances &tolerances) noexcept
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
            write_product(e, scale, point, direction, tolerances, workers_[worker].front());
        }
    }
}

void Hessian::values(std::span<const Real> point,
                     std::span<const Real> weights,
                     const Tolerances &tolerances,
                     std::span<Real> out,
                     Pool &pool)
{
    check(point, weights);
    check_size("out", out.size(), lower_.rows.size(), "the pattern size");
    const std::span<const Real> used = point.first(to_size(tape_.variables()));
    weigh(weights);
    auto fill = [&](auto chunk, auto worker) noexcept
    { fill_blocks(chunk, worker, used, tolerances); };
    pool.run(workers_.size(), chunks_.size() - 1, fill);
    std::ranges::fill(out, 0.0);
    gather(std::span<const Real>(parts_), std::span<const Offset>(map_.blocks), out);
}

void Hessian::product(std::span<const Real> point,
                      std::span<const Real> weights,
                      std::span<const Real> direction,
                      const Tolerances &tolerances,
                      std::span<Real> out,
                      Pool &pool)
{
    check(point, weights);
    check_size("direction", direction.size(), point.size(), "the point size");
    check_size("out", out.size(), point.size(), "the point size");
    const std::span<const Real> used = point.first(to_size(tape_.variables()));
    weigh(weights);
    Products task{.hessian = *this,
                  .point = used,
                  .direction = direction,
                  .tolerances = tolerances};
    pool.run(workers_.size(), chunks_.size() - 1, task);
    std::ranges::fill(out, 0.0);
    const std::span<const Index> variables = tape_.pattern().variables;
    gather(std::span<const Real>(parts_), variables, out);
}

}
