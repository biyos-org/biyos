#include "expr/sparsity/memo.hpp"

#include "core/numbers.hpp"
#include "expr/sparsity/joiner.hpp"
#include "expr/sparsity/reach.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <functional>
#include <ranges>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::size_t kStamp = 0;
constexpr std::size_t kItems = 1;
constexpr std::size_t kLinks = 2;
constexpr std::size_t kHeader = 3;

template <typename Item>
constexpr std::size_t kWidth = (sizeof(Item) + sizeof(Offset) - 1) / sizeof(Offset);

constexpr Bits kSeed = 14695981039346656037ULL;
constexpr Bits kPrime = 1099511628211ULL;

Bits hash_of(std::span<const Offset> links)
{
    Bits hash = kSeed;
    for (const Offset link : links)
    {
        hash = (hash ^ static_cast<Bits>(link)) * kPrime;
    }
    return hash;
}

template <typename T> void append(std::vector<T> &items, std::span<const T> more)
{
    items.insert(items.end(), more.begin(), more.end());
}

void pack(std::vector<Offset> &cells, std::span<const Index> items)
{
    cells.insert(cells.end(), items.begin(), items.end());
}

void pack(std::vector<Offset> &cells, std::span<const Range> items)
{
    for (const Range &item : items)
    {
        cells.push_back(item.begin);
        cells.push_back(item.end);
    }
}

template <typename Item> Item decode(std::span<const Offset> cells);

template <> Index decode<Index>(std::span<const Offset> cells)
{
    return static_cast<Index>(cells[0]);
}

template <> Range decode<Range>(std::span<const Offset> cells)
{
    return {.begin = cells[0], .end = cells[1]};
}

template <typename Item, typename Sink> void unpack(std::span<const Offset> cells, Sink &sink)
{
    for (std::size_t k = cells.size(); k > 0; k -= kWidth<Item>)
    {
        sink.push_back(decode<Item>(cells.subspan(k - kWidth<Item>)));
    }
}

template <typename Item, typename Sink>
void hand(std::span<const Offset> cells, Bits mask, std::span<Sink> sinks)
{
    if (cells.empty())
    {
        return;
    }
    for (Bits rest = mask; rest != 0; rest &= rest - 1)
    {
        unpack<Item>(cells, sinks[static_cast<std::size_t>(std::countr_zero(rest))]);
    }
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
    if (own.empty())
    {
        return share(links);
    }
    return store(own, links);
}

template <typename Item> void Memos<Item>::seed(std::span<const Offset> memos, std::size_t lane)
{
    const Bits bit = Bits{1} << lane;
    for (const Offset memo : memos | std::views::reverse)
    {
        if (mark(memo, bit))
        {
            seeds_.push_back(memo);
        }
    }
}

template <typename Item> template <typename Sink> void Memos<Item>::walk(std::span<Sink> sinks)
{
    order();
    while (taken_ < seeds_.size() || !heap_.empty())
    {
        take(next(), sinks);
    }
    seeds_.clear();
    taken_ = 0;
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
        size += item_count(link);
    }
    return size <= kFlat;
}

template <typename Item>
Offset Memos<Item>::flatten(std::span<const Item> own, std::span<const Offset> links)
{
    pieces_.assign(own.begin(), own.end());
    for (const Offset link : links)
    {
        unpack<Item>(item_cells(link), pieces_);
    }
    settled_.clear();
    settle(pieces_, settled_);
    return store(settled_, {});
}

template <typename Item> Offset Memos<Item>::share(std::span<const Offset> links)
{
    const Bits hash = hash_of(links);
    const auto [first, last] = shared_.equal_range(hash);
    for (const auto &[_, memo] : std::ranges::subrange(first, last))
    {
        if (std::ranges::equal(links_of(memo), links))
        {
            return memo;
        }
    }
    const Offset memo = store({}, links);
    shared_.emplace(hash, memo);
    return memo;
}

template <typename Item>
Offset Memos<Item>::store(std::span<const Item> own, std::span<const Offset> links)
{
    const std::size_t start = cells_.size();
    cells_.resize(start + kHeader);
    cells_[start + kItems] = to_offset(own.size());
    cells_[start + kLinks] = to_offset(links.size());
    pack(cells_, own);
    append(cells_, links);
    return to_offset(start);
}

template <typename Item> std::size_t Memos<Item>::item_count(Offset memo) const noexcept
{
    return to_size(cells_[to_size(memo) + kItems]);
}

template <typename Item> std::size_t Memos<Item>::link_count(Offset memo) const noexcept
{
    return to_size(cells_[to_size(memo) + kLinks]);
}

template <typename Item> std::span<const Offset> Memos<Item>::item_cells(Offset memo) const noexcept
{
    return std::span(cells_).subspan(to_size(memo) + kHeader, item_count(memo) * kWidth<Item>);
}

template <typename Item> std::span<const Offset> Memos<Item>::links_of(Offset memo) const noexcept
{
    const std::size_t start = to_size(memo) + kHeader + item_count(memo) * kWidth<Item>;
    return std::span(cells_).subspan(start, link_count(memo));
}

template <typename Item> Offset &Memos<Item>::stamp(Offset memo) noexcept
{
    return cells_[to_size(memo) + kStamp];
}

template <typename Item> bool Memos<Item>::mark(Offset memo, Bits mask)
{
    Offset &cell = stamp(memo);
    const bool fresh = cell == 0;
    cell = static_cast<Offset>(static_cast<Bits>(cell) | mask);
    return fresh;
}

template <typename Item> void Memos<Item>::order()
{
    if (!std::ranges::is_sorted(seeds_, std::ranges::greater{}))
    {
        std::ranges::sort(seeds_, std::ranges::greater{});
    }
}

template <typename Item> Offset Memos<Item>::next()
{
    if (heap_.empty() || (taken_ < seeds_.size() && seeds_[taken_] > heap_.front()))
    {
        return seeds_[taken_++];
    }
    std::ranges::pop_heap(heap_);
    const Offset memo = heap_.back();
    heap_.pop_back();
    return memo;
}

template <typename Item>
template <typename Sink>
void Memos<Item>::take(Offset memo, std::span<Sink> sinks)
{
    Offset &cell = stamp(memo);
    const auto mask = static_cast<Bits>(cell);
    cell = 0;
    hand<Item>(item_cells(memo), mask, sinks);
    for (const Offset link : links_of(memo))
    {
        if (mark(link, mask))
        {
            heap_.push_back(link);
            std::ranges::push_heap(heap_);
        }
    }
}

template class Memos<Index>;
template class Memos<Range>;
template void Memos<Index>::walk(std::span<std::vector<Index>> sinks);
template void Memos<Range>::walk(std::span<Joiner> sinks);

}
