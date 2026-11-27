#include "expr/sparsity/joiner.hpp"

#include "core/numbers.hpp"
#include "expr/sparsity/radix.hpp"
#include "expr/sparsity/reach.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

void settle(std::vector<Index> &items, std::vector<Index> &into)
{
    std::ranges::sort(items);
    const auto last = std::ranges::unique(items).begin();
    into.insert(into.end(), items.begin(), last);
}

void settle(std::vector<Range> &items, std::vector<Range> &into)
{
    std::ranges::sort(items, {}, &Range::begin);
    join(items, into);
}

void join(std::span<const Range> items, std::vector<Range> &into)
{
    const std::size_t start = into.size();
    for (const Range &item : items)
    {
        if (into.size() > start && item.begin <= into.back().end)
        {
            into.back().end = (std::max)(into.back().end, item.end);
            continue;
        }
        into.push_back(item);
    }
}

void Joiner::start(std::span<const Range> own)
{
    runs_.assign(own.begin(), own.end());
    sorted_ = own.empty();
}

void Joiner::push_back(Range run)
{
    sorted_ = sorted_ && follows(run);
    if (sorted_ && !runs_.empty() && run.end >= runs_.back().begin)
    {
        runs_.back().begin = run.begin;
        return;
    }
    runs_.push_back(run);
}

void Joiner::finish(Radix &radix, std::vector<Range> &into)
{
    if (sorted_)
    {
        into.insert(into.end(), runs_.rbegin(), runs_.rend());
        return;
    }
    radix.sort(runs_);
    join(runs_, into);
}

bool Joiner::follows(Range run) const noexcept
{
    return runs_.empty() || (run.begin <= runs_.back().begin && run.end <= runs_.back().end);
}

}
