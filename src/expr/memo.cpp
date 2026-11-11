#include "expr/memo.hpp"

#include "core/index.hpp"
#include "expr/plan.hpp"
#include "expr/reach.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace biyos
{

namespace
{

constexpr std::size_t kFlat = 64;

std::size_t at(Offset value) noexcept
{
    return static_cast<std::size_t>(value);
}

Offset count_of(std::size_t value) noexcept
{
    return static_cast<Offset>(value);
}

template <typename T> void append(std::vector<T> &items, std::span<const T> more)
{
    items.insert(items.end(), more.begin(), more.end());
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
    const std::size_t first = into.size();
    for (const Range &item : items)
    {
        if (into.size() > first && item.begin <= into.back().end)
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
        if (stamps_[at(next)] == walk_)
        {
            continue;
        }
        stamps_[at(next)] = walk_;
        append(items, items_of(memos_[at(next)]));
        append(stack_, links_of(memos_[at(next)]));
    }
}

template <typename Item>
bool Memos<Item>::small(std::span<const Item> own, std::span<const Offset> links) const noexcept
{
    std::size_t size = own.size();
    for (const Offset link : links)
    {
        const Memo &memo = memos_[at(link)];
        if (!memo.flat)
        {
            return false;
        }
        size += at(memo.item_count);
    }
    return size <= kFlat;
}

template <typename Item>
Offset Memos<Item>::flatten(std::span<const Item> own, std::span<const Offset> links)
{
    pieces_.assign(own.begin(), own.end());
    for (const Offset link : links)
    {
        append(pieces_, items_of(memos_[at(link)]));
    }
    const std::size_t first = items_.size();
    settle(pieces_, items_);
    return add({
        .item_start = count_of(first),
        .item_count = count_of(items_.size() - first),
        .link_start = 0,
        .link_count = 0,
        .flat = true,
    });
}

template <typename Item>
Offset Memos<Item>::store(std::span<const Item> own, std::span<const Offset> links)
{
    const Memo memo{
        .item_start = count_of(items_.size()),
        .item_count = count_of(own.size()),
        .link_start = count_of(links_.size()),
        .link_count = count_of(links.size()),
        .flat = false,
    };
    append(items_, own);
    append(links_, links);
    return add(memo);
}

template <typename Item> Offset Memos<Item>::add(Memo memo)
{
    memos_.push_back(memo);
    stamps_.push_back(kUnset);
    return count_of(memos_.size()) - 1;
}

template <typename Item>
std::span<const Item> Memos<Item>::items_of(const Memo &memo) const noexcept
{
    return std::span(items_).subspan(at(memo.item_start), at(memo.item_count));
}

template <typename Item>
std::span<const Offset> Memos<Item>::links_of(const Memo &memo) const noexcept
{
    return std::span(links_).subspan(at(memo.link_start), at(memo.link_count));
}

template class Memos<Index>;
template class Memos<Range>;

}
