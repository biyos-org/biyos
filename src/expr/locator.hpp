#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/layout.hpp"
#include "expr/tape.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace biyos
{

class Locator
{
  public:
    Locator(const Graph &graph, const Layout &layout);

    void locate(std::span<const Offset> outputs, Block &block);

  private:
    struct Runs
    {
        Offset first;
        Offset count;
    };

    void count(std::span<const Offset> outputs);
    void use(Offset node, Offset times);
    void join(std::size_t position);
    void enter(Offset node, std::size_t walk);
    void keep(std::size_t position);
    std::span<const Interval> runs_of(Offset node) const noexcept;

    const Graph &graph_;
    const Layout &layout_;
    std::vector<Offset> uses_;
    std::vector<Runs> joins_;
    std::vector<Interval> runs_;
    std::vector<Interval> pieces_;
    std::vector<std::size_t> seen_;
    std::vector<Offset> stack_;
};

}
