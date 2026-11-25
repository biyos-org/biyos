#include "expr/tape/locator.hpp"

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/layout.hpp"
#include "expr/tape/plan.hpp"
#include "expr/tape/tape.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::uint8_t kOutput = 2;
constexpr std::uint8_t kMany = 2;
constexpr std::size_t kSmall = 8;
constexpr std::size_t kWide = 16;
constexpr std::size_t kNoWalk = (std::numeric_limits<std::size_t>::max)();
constexpr std::uint32_t kOwn = (std::numeric_limits<std::uint32_t>::max)();

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

Offset count_of(std::size_t value) noexcept
{
    return static_cast<Offset>(value);
}

Run point(std::size_t position) noexcept
{
    const auto begin = static_cast<std::uint32_t>(position);
    return {.begin = begin, .end = begin + 1};
}

Range range_of(Run run) noexcept
{
    return {.begin = run.begin, .end = run.end};
}

Run run_of(Range range) noexcept
{
    return {
        .begin = static_cast<std::uint32_t>(range.begin),
        .end = static_cast<std::uint32_t>(range.end),
    };
}

void merge(std::span<const Run> pieces, std::vector<Run> &into)
{
    const std::size_t first = into.size();
    for (const Run &piece : pieces)
    {
        if (into.size() > first && piece.begin <= into.back().end)
        {
            into.back().end = (std::max)(into.back().end, piece.end);
            continue;
        }
        into.push_back(piece);
    }
}

bool covers(std::span<const Run> runs, Offset link) noexcept
{
    const auto position = static_cast<std::uint32_t>(link);
    const auto found = std::ranges::upper_bound(runs, position, {}, &Run::end);
    return found != runs.end() && found->begin <= position;
}

}

Locator::Locator(const Graph &graph, std::span<const Offset> pinned, const Layout &layout)
    : graph_(graph), pinned_(pinned), layout_(layout)
{
}

void Locator::locate(std::span<const Offset> outputs, Block &block)
{
    const std::size_t size = layout_.nodes().size();
    count(outputs);
    seen_.assign(size, kNoWalk);
    runs_.clear();
    run_starts_.assign(size + 1, 0);
    links_.clear();
    link_starts_.assign(size + 1, 0);
    for (std::size_t position = 0; position < size; ++position)
    {
        open(position);
        if (uses_[position] != 1)
        {
            join(position);
        }
    }
    open(size);
    block.starts.push_back(0);
    for (std::size_t k = 0; k < outputs.size(); ++k)
    {
        gather(outputs[k], size + k, block);
    }
}

void Locator::count(std::span<const Offset> outputs)
{
    uses_.assign(layout_.nodes().size(), 0);
    outputs_.assign(layout_.nodes().size(), false);
    for (const Offset node : layout_.nodes())
    {
        for (const Offset child : graph_.children(node))
        {
            use(child, 1);
        }
    }
    for (const Offset output : outputs)
    {
        use(output, kOutput);
        mark(output);
    }
}

void Locator::use(Offset node, std::uint8_t times)
{
    const Offset position = layout_.position(node);
    if (position != kUnset)
    {
        std::uint8_t &uses = uses_[at(position)];
        uses = (std::min)(static_cast<std::uint8_t>(uses + times), kMany);
    }
}

void Locator::mark(Offset output)
{
    const Offset position = layout_.position(output);
    if (position != kUnset)
    {
        outputs_[at(position)] = true;
    }
}

void Locator::open(std::size_t position)
{
    run_starts_[position] = count_of(runs_.size());
    link_starts_[position] = count_of(links_.size());
}

void Locator::join(std::size_t position)
{
    pieces_.clear();
    met_.clear();
    found_.clear();
    pieces_.push_back(point(position));
    stack_.push_back(layout_.nodes()[position]);
    while (!stack_.empty())
    {
        const Offset node = stack_.back();
        stack_.pop_back();
        for (const Offset child : graph_.children(node))
        {
            enter(child, position);
        }
    }
    if (outputs_[position] || met_.empty())
    {
        std::ranges::sort(pieces_, {}, &Run::begin);
        merge(pieces_, runs_);
    }
    else
    {
        choose(position);
    }
    keep(position);
}

void Locator::enter(Offset node, std::size_t walk)
{
    const Offset position = layout_.position(node);
    if (position == kUnset)
    {
        return;
    }
    const std::size_t place = at(position);
    if (uses_[place] == 1)
    {
        pieces_.push_back(point(place));
        stack_.push_back(node);
        return;
    }
    if (seen_[place] != walk && pinned_[at(node)] == kUnset)
    {
        seen_[place] = walk;
        meet(place, walk);
    }
}

void Locator::meet(std::size_t position, std::size_t walk)
{
    if (!outputs_[walk])
    {
        met_.push_back(count_of(position));
        return;
    }
    if (size_of(position) > kSmall)
    {
        found_.push_back(count_of(position));
        return;
    }
    const std::span<const Run> runs = runs_of(position);
    pieces_.insert(pieces_.end(), runs.begin(), runs.end());
    adopt(position, walk);
}

void Locator::adopt(std::size_t position, std::size_t walk)
{
    for (const Offset link : links_of(position))
    {
        if (seen_[at(link)] != walk)
        {
            seen_[at(link)] = walk;
            found_.push_back(link);
        }
    }
}

void Locator::choose(std::size_t walk)
{
    if (fits())
    {
        absorb(walk);
        return;
    }
    if (met_.size() == 1)
    {
        single(walk);
        return;
    }
    const std::size_t own = tag();
    if (whole_.size() + widen() > kSmall)
    {
        const std::size_t whole = whole_.size() + linked(walk, whole_);
        if (whole > kSmall && narrower(walk, own, whole))
        {
            return;
        }
    }
    apply(walk, whole_);
}

bool Locator::fits() const noexcept
{
    std::size_t size = pieces_.size();
    for (const Offset child : met_)
    {
        size += size_of(at(child));
    }
    return size <= kSmall;
}

void Locator::absorb(std::size_t walk)
{
    for (const Offset child : met_)
    {
        const std::span<const Run> runs = runs_of(at(child));
        pieces_.insert(pieces_.end(), runs.begin(), runs.end());
        adopt(at(child), walk);
    }
    std::ranges::sort(pieces_, {}, &Run::begin);
    merge(pieces_, runs_);
}

void Locator::single(std::size_t walk)
{
    const std::size_t child = at(met_.front());
    const std::size_t size = size_of(child);
    std::ranges::sort(pieces_, {}, &Run::begin);
    chosen_.clear();
    merge(pieces_, chosen_);
    if (chosen_.size() + size <= kSmall || (size <= kWide && touches(runs_of(child))))
    {
        pieces_.assign(chosen_.begin(), chosen_.end());
        absorb(walk);
        return;
    }
    found_.push_back(met_.front());
    runs_.insert(runs_.end(), chosen_.begin(), chosen_.end());
}

bool Locator::touches(std::span<const Run> runs) const noexcept
{
    for (const Run run : runs)
    {
        const auto found = std::ranges::lower_bound(chosen_, run.begin, {}, &Run::end);
        if (found == chosen_.end() || found->begin > run.end)
        {
            return false;
        }
    }
    return true;
}

bool Locator::narrower(std::size_t walk, std::size_t own, std::size_t whole)
{
    pick(own);
    const std::span<const Run> runs = selected();
    if (runs.size() + linked(walk, runs) >= whole)
    {
        widen();
        return false;
    }
    apply(walk, runs);
    return true;
}

std::size_t Locator::widen()
{
    taken_.assign(met_.size(), false);
    std::size_t links = 0;
    for (std::size_t k = 0; k < met_.size(); ++k)
    {
        const std::size_t position = at(met_[k]);
        taken_[k] = size_of(position) <= kWide;
        links += taken_[k] ? links_of(position).size() : 1;
    }
    return links;
}

void Locator::pick(std::size_t held)
{
    std::size_t links = 0;
    for (std::size_t k = 0; k < met_.size(); ++k)
    {
        const std::size_t position = at(met_[k]);
        const std::size_t size = size_of(position);
        const bool loose = loose_[k];
        taken_[k] = size <= kSmall && (!loose || held + links + size <= kSmall);
        if (!taken_[k])
        {
            ++links;
            continue;
        }
        held += loose ? runs_of(position).size() : 0;
        links += links_of(position).size();
    }
}

std::span<const Run> Locator::selected()
{
    pieces_.clear();
    for (const Tagged &piece : tagged_)
    {
        if (piece.source == kOwn || taken_[piece.source])
        {
            pieces_.push_back({.begin = piece.begin, .end = piece.end});
        }
    }
    chosen_.clear();
    merge(pieces_, chosen_);
    return chosen_;
}

std::size_t Locator::linked(std::size_t walk, std::span<const Run> runs)
{
    adopted_.clear();
    for (std::size_t k = 0; k < met_.size(); ++k)
    {
        const std::size_t position = at(met_[k]);
        if (!taken_[k])
        {
            adopted_.push_back(met_[k]);
            continue;
        }
        for (const Offset link : links_of(position))
        {
            if (seen_[at(link)] != walk)
            {
                adopted_.push_back(link);
            }
        }
    }
    std::ranges::sort(adopted_);
    const auto repeated = std::ranges::unique(adopted_);
    std::size_t count = 0;
    for (const Offset link : std::span(adopted_.begin(), repeated.begin()))
    {
        count += covers(runs, link) ? 0U : 1U;
    }
    return count;
}

void Locator::apply(std::size_t walk, std::span<const Run> runs)
{
    for (std::size_t k = 0; k < met_.size(); ++k)
    {
        if (taken_[k])
        {
            adopt(at(met_[k]), walk);
        }
        else
        {
            found_.push_back(met_[k]);
        }
    }
    runs_.insert(runs_.end(), runs.begin(), runs.end());
}

std::size_t Locator::tag()
{
    tagged_.clear();
    for (const Run run : pieces_)
    {
        tagged_.push_back({.begin = run.begin, .end = run.end, .source = kOwn});
    }
    for (std::size_t k = 0; k < met_.size(); ++k)
    {
        const std::size_t position = at(met_[k]);
        if (size_of(position) <= kWide)
        {
            for (const Run run : runs_of(position))
            {
                const auto source = static_cast<std::uint32_t>(k);
                tagged_.push_back({.begin = run.begin, .end = run.end, .source = source});
            }
        }
    }
    std::ranges::sort(tagged_, {}, &Tagged::begin);
    loose_.assign(met_.size(), false);
    whole_.clear();
    std::size_t own = 0;
    for (std::size_t first = 0; first < tagged_.size();)
    {
        first = close(first, own);
    }
    return own;
}

std::size_t Locator::close(std::size_t first, std::size_t &own)
{
    std::uint32_t end = tagged_[first].end;
    bool held = false;
    std::size_t last = first;
    for (; last < tagged_.size() && tagged_[last].begin <= end; ++last)
    {
        end = (std::max)(end, tagged_[last].end);
        held = held || tagged_[last].source == kOwn;
    }
    whole_.push_back({.begin = tagged_[first].begin, .end = end});
    own += held ? 1U : 0U;
    for (std::size_t k = first; k < last && !held; ++k)
    {
        loose_[tagged_[k].source] = true;
    }
    return last;
}

void Locator::keep(std::size_t position)
{
    const std::span<const Run> runs = std::span(runs_).subspan(at(run_starts_[position]));
    for (const Offset link : found_)
    {
        if (!covers(runs, link))
        {
            links_.push_back(link);
        }
    }
}

std::size_t Locator::size_of(std::size_t position) const noexcept
{
    return runs_of(position).size() + links_of(position).size();
}

void Locator::gather(Offset output, std::size_t walk, Block &block)
{
    const Offset position = layout_.position(output);
    if (position != kUnset)
    {
        append(at(position), walk, block.runs);
    }
    block.starts.push_back(count_of(block.runs.size()));
}

void Locator::append(std::size_t position, std::size_t walk, std::vector<Run> &into)
{
    if (links_of(position).empty())
    {
        merge(runs_of(position), into);
        return;
    }
    gathered_.clear();
    collect(position, walk);
    radix_.sort(gathered_);
    pieces_.clear();
    for (const Range piece : gathered_)
    {
        pieces_.push_back(run_of(piece));
    }
    merge(pieces_, into);
}

void Locator::collect(std::size_t position, std::size_t walk)
{
    pending_.push_back(count_of(position));
    while (!pending_.empty())
    {
        const std::size_t next = at(pending_.back());
        pending_.pop_back();
        for (const Run run : runs_of(next))
        {
            gathered_.push_back(range_of(run));
        }
        for (const Offset link : links_of(next))
        {
            if (seen_[at(link)] != walk)
            {
                seen_[at(link)] = walk;
                pending_.push_back(link);
            }
        }
    }
}

std::span<const Run> Locator::runs_of(std::size_t position) const noexcept
{
    const Offset first = run_starts_[position];
    return std::span(runs_).subspan(at(first), at(run_starts_[position + 1] - first));
}

std::span<const Offset> Locator::links_of(std::size_t position) const noexcept
{
    const Offset first = link_starts_[position];
    return std::span(links_).subspan(at(first), at(link_starts_[position + 1] - first));
}

}
