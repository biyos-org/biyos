#include "expr/sparsity/memo.hpp"

#include "core/numbers.hpp"
#include "expr/sparsity/joiner.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/plan.hpp"

#include <algorithm>
#include <cstddef>
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

void unpack(std::span<const Offset> cells, std::vector<Index> &items)
{
    for (const Offset cell : cells)
    {
        items.push_back(static_cast<Index>(cell));
    }
}

void unpack(std::span<const Offset> cells, std::vector<Range> &items)
{
    for (std::size_t k = 0; k < cells.size(); k += kWidth<Range>)
    {
        items.push_back({.begin = cells[k], .end = cells[k + 1]});
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
        unpack(item_cells(next), items);
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
        unpack(item_cells(link), pieces_);
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
    cells_[start + kStamp] = kUnset;
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

template class Memos<Index>;
template class Memos<Range>;

}
