#include "expr/sparsity/memo.hpp"

#include "core/numbers.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/plan.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::size_t kFlat = 64;
constexpr std::size_t kStamp = 0;
constexpr std::size_t kCount = 1;
constexpr std::size_t kLinks = 2;
constexpr std::size_t kHeader = 3;

template <typename Item>
constexpr std::size_t kWidth = (sizeof(Item) + sizeof(Offset) - 1) / sizeof(Offset);

template <typename T> void append(std::vector<T> &items, std::span<const T> more)
{
    items.insert(items.end(), more.begin(), more.end());
}

void put(std::vector<Offset> &cells, std::span<const Index> items)
{
    cells.insert(cells.end(), items.begin(), items.end());
}

void put(std::vector<Offset> &cells, std::span<const Range> items)
{
    for (const Range &item : items)
    {
        cells.push_back(item.begin);
        cells.push_back(item.end);
    }
}

void read(std::span<const Offset> cells, std::vector<Index> &items)
{
    for (const Offset cell : cells)
    {
        items.push_back(static_cast<Index>(cell));
    }
}

void read(std::span<const Offset> cells, std::vector<Range> &items)
{
    for (std::size_t k = 0; k < cells.size(); k += 2)
    {
        items.push_back({.begin = cells[k], .end = cells[k + 1]});
    }
}

}

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

template <typename Item>
Offset Memos<Item>::keep(std::span<const Item> own, std::span<const Offset> links)
{
    if (small(own, links))
    {
        return flatten(own, links);
    }
    if (own.empty() && links.size() == 1)
    {
        return links.front();
    }
    return store(own, links);
}

template <typename Item>
void Memos<Item>::gather(std::span<const Offset> memos, std::vector<Item> &items)
{
    ++walk_;
    append(stack_, memos);
    while (!stack_.empty())
    {
        const Offset next = stack_.back();
        stack_.pop_back();
        Offset &stamp = cells_[to_size(next) + kStamp];
        if (stamp == walk_)
        {
            continue;
        }
        stamp = walk_;
        read(items_of(next), items);
        append(stack_, links_of(next));
    }
}

template <typename Item>
bool Memos<Item>::small(std::span<const Item> own, std::span<const Offset> links) const noexcept
{
    std::size_t size = own.size();
    for (const Offset link : links)
    {
        if (!links_of(link).empty())
        {
            return false;
        }
        size += count_of(link);
    }
    return size <= kFlat;
}

template <typename Item>
Offset Memos<Item>::flatten(std::span<const Item> own, std::span<const Offset> links)
{
    pieces_.assign(own.begin(), own.end());
    for (const Offset link : links)
    {
        read(items_of(link), pieces_);
    }
    settled_.clear();
    settle(pieces_, settled_);
    return store(settled_, {});
}

template <typename Item>
Offset Memos<Item>::store(std::span<const Item> own, std::span<const Offset> links)
{
    const Offset memo = to_offset(cells_.size());
    cells_.push_back(kUnset);
    cells_.push_back(to_offset(own.size()));
    cells_.push_back(to_offset(links.size()));
    put(cells_, own);
    append(cells_, links);
    return memo;
}

template <typename Item> std::size_t Memos<Item>::count_of(Offset memo) const noexcept
{
    return to_size(cells_[to_size(memo) + kCount]);
}

template <typename Item> std::span<const Offset> Memos<Item>::items_of(Offset memo) const noexcept
{
    return std::span(cells_).subspan(to_size(memo) + kHeader, count_of(memo) * kWidth<Item>);
}

template <typename Item> std::span<const Offset> Memos<Item>::links_of(Offset memo) const noexcept
{
    const std::size_t start = to_size(memo) + kHeader + count_of(memo) * kWidth<Item>;
    return std::span(cells_).subspan(start, to_size(cells_[to_size(memo) + kLinks]));
}

template class Memos<Index>;
template class Memos<Range>;

}
