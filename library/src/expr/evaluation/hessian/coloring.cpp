#include "expr/evaluation/hessian/coloring.hpp"

#include "core/memory/slice.hpp"
#include "core/numbers.hpp"
#include "expr/evaluation/hessian/elements.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

namespace biyos
{

namespace
{

constexpr Index kNone = -1;
constexpr Offset kNoEdge = -1;

struct Neighbour
{
    Index vertex;
    Offset edge;
};

struct Adjacency
{
    std::vector<Offset> starts;
    std::vector<Neighbour> neighbours;

    std::span<const Neighbour> of(Index vertex) const noexcept
    {
        return slice(std::span<const Neighbour>(neighbours), starts, to_size(vertex));
    }

    Offset degree(Index vertex) const noexcept
    {
        return starts[to_size(vertex) + 1] - starts[to_size(vertex)];
    }
};

bool off_diagonal(Entry entry) noexcept
{
    return entry.row != entry.column;
}

std::vector<Offset> degrees_of(std::span<const Entry> entries, std::size_t size)
{
    std::vector<Offset> degrees(size, 0);
    for (const Entry entry : entries)
    {
        if (off_diagonal(entry))
        {
            ++degrees[to_size(entry.row)];
            ++degrees[to_size(entry.column)];
        }
    }
    return degrees;
}

Adjacency adjacency_of(std::span<const Entry> entries, std::size_t size)
{
    const std::vector<Offset> degrees = degrees_of(entries, size);
    Adjacency adjacency{.starts = std::vector<Offset>(size + 1, 0), .neighbours = {}};
    std::inclusive_scan(degrees.begin(), degrees.end(), adjacency.starts.begin() + 1);
    adjacency.neighbours.resize(to_size(adjacency.starts.back()));
    std::vector<Offset> next(adjacency.starts.begin(), adjacency.starts.end() - 1);
    for (std::size_t k = 0; k < entries.size(); ++k)
    {
        const Entry entry = entries[k];
        if (off_diagonal(entry))
        {
            const Offset edge = to_offset(k);
            adjacency.neighbours[to_size(next[to_size(entry.row)]++)] = {entry.column, edge};
            adjacency.neighbours[to_size(next[to_size(entry.column)]++)] = {entry.row, edge};
        }
    }
    return adjacency;
}

std::vector<Index> order_of(const Adjacency &adjacency, std::size_t size)
{
    std::vector<Index> order(size);
    std::iota(order.begin(), order.end(), 0);
    std::ranges::stable_sort(order, {}, [&](Index vertex) { return adjacency.degree(vertex); });
    return order;
}

class Star
{
  public:
    Star(const Adjacency &adjacency, std::size_t size, std::size_t edges);

    void color(Index vertex);
    std::vector<Index> colors() &&;

  private:
    struct First
    {
        Index vertex;
        Index neighbour;
        Offset edge;
    };

    bool colored(Index vertex) const noexcept;
    void forbid(Index vertex, Neighbour neighbour);
    void treat(Index vertex, Index neighbour);
    Index pick(Index vertex) const noexcept;
    void join(Index vertex, Neighbour neighbour);
    Offset matching(Index vertex, Index neighbour) const noexcept;

    const Adjacency &adjacency_;
    std::vector<Index> colors_;
    std::vector<Index> forbidden_;
    std::vector<Index> treated_;
    std::vector<First> first_;
    std::vector<Offset> star_;
    std::vector<Index> hub_;
    Offset stars_ = 0;
};

Star::Star(const Adjacency &adjacency, std::size_t size, std::size_t edges)
    : adjacency_(adjacency), colors_(size, kNone), forbidden_(size, kNone), treated_(size, kNone),
      first_(size, First{.vertex = kNone, .neighbour = kNone, .edge = kNoEdge}),
      star_(edges, kNoEdge), hub_(edges, kNone)
{
}

void Star::color(Index vertex)
{
    for (const Neighbour neighbour : adjacency_.of(vertex))
    {
        if (colored(neighbour.vertex))
        {
            forbid(vertex, neighbour);
        }
    }
    colors_[to_size(vertex)] = pick(vertex);
    for (const Neighbour neighbour : adjacency_.of(vertex))
    {
        if (colored(neighbour.vertex))
        {
            join(vertex, neighbour);
        }
    }
}

std::vector<Index> Star::colors() &&
{
    return std::move(colors_);
}

bool Star::colored(Index vertex) const noexcept
{
    return colors_[to_size(vertex)] != kNone;
}

void Star::forbid(Index vertex, Neighbour neighbour)
{
    const auto color = to_size(colors_[to_size(neighbour.vertex)]);
    forbidden_[color] = vertex;
    const First first = first_[color];
    if (first.vertex == vertex)
    {
        if (treated_[to_size(first.neighbour)] != vertex)
        {
            treat(vertex, first.neighbour);
        }
        treat(vertex, neighbour.vertex);
        return;
    }
    first_[color] = {.vertex = vertex, .neighbour = neighbour.vertex, .edge = neighbour.edge};
    for (const Neighbour next : adjacency_.of(neighbour.vertex))
    {
        if (colored(next.vertex) && hub_[to_size(star_[to_size(next.edge)])] == next.vertex)
        {
            forbidden_[to_size(colors_[to_size(next.vertex)])] = vertex;
        }
    }
}

void Star::treat(Index vertex, Index neighbour)
{
    for (const Neighbour next : adjacency_.of(neighbour))
    {
        if (colored(next.vertex))
        {
            forbidden_[to_size(colors_[to_size(next.vertex)])] = vertex;
        }
    }
    treated_[to_size(neighbour)] = vertex;
}

Index Star::pick(Index vertex) const noexcept
{
    Index color = 0;
    while (forbidden_[to_size(color)] == vertex)
    {
        ++color;
    }
    return color;
}

void Star::join(Index vertex, Neighbour neighbour)
{
    const auto edge = to_size(neighbour.edge);
    const Offset shared = matching(vertex, neighbour.vertex);
    if (shared != kNoEdge)
    {
        hub_[to_size(star_[to_size(shared)])] = neighbour.vertex;
        star_[edge] = star_[to_size(shared)];
        return;
    }
    const First first = first_[to_size(colors_[to_size(neighbour.vertex)])];
    if (first.vertex == vertex && first.neighbour != neighbour.vertex)
    {
        hub_[to_size(star_[to_size(first.edge)])] = vertex;
        star_[edge] = star_[to_size(first.edge)];
        return;
    }
    star_[edge] = stars_++;
}

Offset Star::matching(Index vertex, Index neighbour) const noexcept
{
    const Index color = colors_[to_size(vertex)];
    for (const Neighbour next : adjacency_.of(neighbour))
    {
        if (next.vertex != vertex && colors_[to_size(next.vertex)] == color)
        {
            return next.edge;
        }
    }
    return kNoEdge;
}

std::vector<Index> rows_of(std::span<const Entry> entries,
                           const Adjacency &adjacency,
                           std::span<const Index> colors)
{
    std::vector<Index> rows(entries.size());
    std::ranges::transform(entries, rows.begin(), &Entry::column);
    std::vector<Index> counts(colors.size(), 0);
    std::vector<Index> stamps(colors.size(), kNone);
    for (Index vertex = 0; vertex < to_index(colors.size()); ++vertex)
    {
        for (const Neighbour neighbour : adjacency.of(vertex))
        {
            const auto color = to_size(colors[to_size(neighbour.vertex)]);
            counts[color] = stamps[color] == vertex ? counts[color] + 1 : 1;
            stamps[color] = vertex;
        }
        for (const Neighbour neighbour : adjacency.of(vertex))
        {
            const auto edge = to_size(neighbour.edge);
            if (entries[edge].row == vertex &&
                counts[to_size(colors[to_size(neighbour.vertex)])] == 1)
            {
                rows[edge] = vertex;
            }
        }
    }
    return rows;
}

std::vector<Read> reads_of(std::span<const Entry> entries,
                           const Adjacency &adjacency,
                           std::span<const Index> colors)
{
    const std::vector<Index> rows = rows_of(entries, adjacency, colors);
    std::vector<Read> reads;
    reads.reserve(entries.size());
    for (std::size_t k = 0; k < entries.size(); ++k)
    {
        const Index row = rows[k];
        const Index other = row == entries[k].row ? entries[k].column : entries[k].row;
        reads.push_back({.part = to_offset(k), .row = row, .color = colors[to_size(other)]});
    }
    std::ranges::stable_sort(reads, {}, &Read::color);
    return reads;
}

}

Offset star_work(std::span<const Entry> entries, std::size_t size)
{
    const std::vector<Offset> degrees = degrees_of(entries, size);
    Offset work = 0;
    for (const Entry entry : entries)
    {
        if (off_diagonal(entry))
        {
            work += (std::min)(degrees[to_size(entry.row)], degrees[to_size(entry.column)]);
        }
    }
    return work;
}

Coloring star_coloring(std::span<const Entry> entries, std::size_t size)
{
    const Adjacency adjacency = adjacency_of(entries, size);
    Star star(adjacency, size, entries.size());
    for (const Index vertex : order_of(adjacency, size))
    {
        star.color(vertex);
    }
    std::vector<Index> colors = std::move(star).colors();
    std::vector<Read> reads = reads_of(entries, adjacency, colors);
    const Index directions = reads.empty() ? 0 : reads.back().color + 1;
    return {.directions = directions, .colors = std::move(colors), .reads = std::move(reads)};
}

}
