#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/plan.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace biyos
{

class Layout
{
  public:
    Layout(const Graph &graph, const Plan &plan);

    void build(Offset block, std::span<const Offset> outputs);
    std::span<const Offset> nodes() const noexcept;
    std::span<const Offset> loads() const noexcept;
    std::uint32_t slot(Offset node) const noexcept;
    std::uint32_t width() const noexcept;

  private:
    struct Step
    {
        Offset node;
        std::size_t next;
    };

    bool computed(Offset node, Offset block) const noexcept;
    void enter(Offset node, Offset block);
    void descend(Offset output, Offset block);
    void collect(Offset block, std::span<const Offset> outputs);
    void number();
    void mark(std::span<const Offset> outputs);
    void allocate();
    void release(Offset node, Offset use);
    std::uint32_t take();
    std::size_t local(Offset node) const noexcept;

    const Graph &graph_;
    const Plan &plan_;
    std::vector<Offset> stamps_;
    std::vector<Offset> local_;
    std::vector<Step> stack_;
    std::vector<Offset> nodes_;
    std::vector<Offset> loads_;
    std::vector<Offset> last_;
    std::vector<std::uint32_t> slots_;
    std::vector<std::uint32_t> free_;
    std::uint32_t width_ = 0;
};

std::uint32_t narrow(std::size_t count);

}
