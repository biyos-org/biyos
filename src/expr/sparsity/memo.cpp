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
        if (stamps_[to_size(next)] == walk_)
        {
            continue;
        }
        stamps_[to_size(next)] = walk_;
        append(items, items_of(memos_[to_size(next)]));
        append(stack_, links_of(memos_[to_size(next)]));
    }
}

template <typename Item>
bool Memos<Item>::small(std::span<const Item> own, std::span<const Offset> links) const noexcept
{
    std::size_t size = own.size();
    for (const Offset link : links)
    {
        const Memo &memo = memos_[to_size(link)];
        if (!memo.flat)
        {
            return false;
        }
        size += to_size(memo.n_items);
    }
    return size <= kFlat;
}

template <typename Item>
Offset Memos<Item>::flatten(std::span<const Item> own, std::span<const Offset> links)
{
    pieces_.assign(own.begin(), own.end());
    for (const Offset link : links)
    {
        append(pieces_, items_of(memos_[to_size(link)]));
    }
    const std::size_t start = items_.size();
    settle(pieces_, items_);
    return add({
        .item_start = to_offset(start),
        .n_items = to_offset(items_.size() - start),
        .link_start = 0,
        .n_links = 0,
        .flat = true,
    });
}

template <typename Item>
Offset Memos<Item>::store(std::span<const Item> own, std::span<const Offset> links)
{
    const Memo memo{
        .item_start = to_offset(items_.size()),
        .n_items = to_offset(own.size()),
        .link_start = to_offset(links_.size()),
        .n_links = to_offset(links.size()),
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
    return to_offset(memos_.size()) - 1;
}

template <typename Item>
std::span<const Item> Memos<Item>::items_of(const Memo &memo) const noexcept
{
    return std::span(items_).subspan(to_size(memo.item_start), to_size(memo.n_items));
}

template <typename Item>
std::span<const Offset> Memos<Item>::links_of(const Memo &memo) const noexcept
{
    return std::span(links_).subspan(to_size(memo.link_start), to_size(memo.n_links));
}

template class Memos<Index>;
template class Memos<Range>;

}
