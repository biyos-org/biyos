#pragma once

#include "core/numbers.hpp"
#include "expr/sparsity/reach.hpp"

#include <cstddef>
#include <limits>
#include <span>
#include <unordered_map>
#include <vector>

namespace biyos
{

template <typename Item> class Memos
{
  public:
    static constexpr std::size_t kFlat = 64;
    static constexpr std::size_t kLanes = std::numeric_limits<Bits>::digits;

    Offset keep(std::span<const Item> own, std::span<const Offset> links);
    void seed(std::span<const Offset> memos, std::size_t lane);
    template <typename Sink> void walk(std::span<Sink> sinks);

  private:
    bool small(std::span<const Item> own, std::span<const Offset> links) const noexcept;
    Offset flatten(std::span<const Item> own, std::span<const Offset> links);
    Offset share(std::span<const Offset> links);
    Offset store(std::span<const Item> own, std::span<const Offset> links);
    std::size_t item_count(Offset memo) const noexcept;
    std::size_t link_count(Offset memo) const noexcept;
    std::span<const Offset> item_cells(Offset memo) const noexcept;
    std::span<const Offset> links_of(Offset memo) const noexcept;
    Offset &stamp(Offset memo) noexcept;
    bool mark(Offset memo, Bits mask);
    void order();
    Offset next();
    template <typename Sink> void take(Offset memo, std::span<Sink> sinks);

    std::vector<Offset> cells_;
    std::unordered_multimap<Bits, Offset> shared_;
    std::vector<Offset> seeds_;
    std::size_t taken_ = 0;
    std::vector<Offset> heap_;
    std::vector<Item> pieces_;
    std::vector<Item> settled_;
};

}
