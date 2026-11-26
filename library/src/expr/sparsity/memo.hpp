#pragma once

#include "core/numbers.hpp"
#include "expr/sparsity/reach.hpp"

#include <cstddef>
#include <span>
#include <unordered_map>
#include <vector>

namespace biyos
{

void settle(std::vector<Index> &items, std::vector<Index> &into);
void settle(std::vector<Range> &items, std::vector<Range> &into);
void join(std::span<const Range> items, std::vector<Range> &into);

template <typename Item> class Memos
{
  public:
    static constexpr std::size_t kFlat = 64;

    Offset keep(std::span<const Item> own, std::span<const Offset> links);
    void gather(std::span<const Offset> memos, std::vector<Item> &items);

  private:
    bool small(std::span<const Item> own, std::span<const Offset> links) const noexcept;
    Offset flatten(std::span<const Item> own, std::span<const Offset> links);
    Offset share(std::span<const Offset> links);
    Offset store(std::span<const Item> own, std::span<const Offset> links);
    std::size_t item_count(Offset memo) const noexcept;
    std::size_t link_count(Offset memo) const noexcept;
    std::span<const Offset> item_cells(Offset memo) const noexcept;
    std::span<const Offset> links_of(Offset memo) const noexcept;

    std::vector<Offset> cells_;
    std::unordered_multimap<Bits, Offset> shared_;
    std::vector<Offset> stack_;
    std::vector<Item> pieces_;
    std::vector<Item> settled_;
    Offset walk_ = 0;
};

}
