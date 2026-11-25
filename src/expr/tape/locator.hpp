#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "expr/sparsity/radix.hpp"
#include "expr/sparsity/reach.hpp"
#include "expr/tape/layout.hpp"
#include "expr/tape/tape.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace biyos
{

class Locator
{
  public:
    Locator(const Graph &graph, std::span<const Offset> pinned, const Layout &layout);

    void locate(std::span<const Offset> outputs, Block &block);

    struct Tagged
    {
        std::uint32_t begin;
        std::uint32_t end;
        std::uint32_t source;
    };

  private:
    void count(std::span<const Offset> outputs);
    void use(Offset node, std::uint8_t times);
    void mark(Offset output);
    void open(std::size_t position);
    void join(std::size_t position);
    void enter(Offset node, std::size_t walk);
    void meet(std::size_t position, std::size_t walk);
    void adopt(std::size_t position, std::size_t walk);
    void choose(std::size_t walk);
    bool fits() const noexcept;
    void absorb(std::size_t walk);
    void single(std::size_t walk);
    bool touches(std::span<const Run> runs) const noexcept;
    bool narrower(std::size_t walk, std::size_t own, std::size_t whole);
    std::size_t widen();
    void pick(std::size_t held);
    std::span<const Run> selected();
    std::size_t linked(std::size_t walk, std::span<const Run> runs);
    void apply(std::size_t walk, std::span<const Run> runs);
    std::size_t tag();
    std::size_t close(std::size_t first, std::size_t &own);
    void keep(std::size_t position);
    std::size_t size_of(std::size_t position) const noexcept;
    void gather(Offset output, std::size_t walk, Block &block);
    void append(std::size_t position, std::size_t walk, std::vector<Run> &into);
    void collect(std::size_t position, std::size_t walk);
    std::span<const Run> runs_of(std::size_t position) const noexcept;
    std::span<const Offset> links_of(std::size_t position) const noexcept;

    const Graph &graph_;
    std::span<const Offset> pinned_;
    const Layout &layout_;
    std::vector<std::uint8_t> uses_;
    std::vector<bool> outputs_;
    std::vector<Run> runs_;
    std::vector<Offset> run_starts_;
    std::vector<Offset> links_;
    std::vector<Offset> link_starts_;
    std::vector<Run> pieces_;
    std::vector<Tagged> tagged_;
    std::vector<bool> loose_;
    std::vector<bool> taken_;
    std::vector<Run> whole_;
    std::vector<Run> chosen_;
    std::vector<Offset> adopted_;
    std::vector<Range> gathered_;
    std::vector<Offset> met_;
    std::vector<Offset> found_;
    std::vector<std::size_t> seen_;
    std::vector<Offset> stack_;
    std::vector<Offset> pending_;
    Radix radix_;
};

}
