#pragma once

#include "core/index.hpp"
#include "expr/reach.hpp"

#include <span>
#include <vector>

namespace biyos
{

void settle(std::vector<Index> &items, std::vector<Index> &into);
void settle(std::vector<Range> &items, std::vector<Range> &into);
void join(std::span<const Range> items, std::vector<Range> &into);

template <typename Item> class Memos
{
  public:
    Offset keep(std::span<const Item> own, std::span<const Offset> links);
    void gather(std::span<const Offset> memos, std::vector<Item> &items);

  private:
    struct Memo
    {
        Offset item_start;
        Offset item_count;
        Offset link_start;
        Offset link_count;
        bool flat;
    };

    bool small(std::span<const Item> own, std::span<const Offset> links) const noexcept;
    Offset flatten(std::span<const Item> own, std::span<const Offset> links);
    Offset store(std::span<const Item> own, std::span<const Offset> links);
    Offset add(Memo memo);
    std::span<const Item> items_of(const Memo &memo) const noexcept;
    std::span<const Offset> links_of(const Memo &memo) const noexcept;

    std::vector<Memo> memos_;
    std::vector<Item> items_;
    std::vector<Offset> links_;
    std::vector<Offset> stamps_;
    std::vector<Offset> stack_;
    std::vector<Item> pieces_;
    Offset walk_ = 0;
};

}
