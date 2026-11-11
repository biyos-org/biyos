#pragma once

#include "core/index.hpp"
#include "expr/graph.hpp"
#include "expr/node.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace biyos
{

constexpr std::size_t kCacheBytes = 32 * 1024;
constexpr Offset kBudget = static_cast<Offset>(kCacheBytes / sizeof(double));

struct Instruction
{
    Operator op;
    std::uint32_t target;
    std::uint32_t first;
    std::uint32_t second;
    std::uint64_t payload;
};

static_assert(sizeof(Instruction) == 24);

struct Interval
{
    std::uint32_t begin;
    std::uint32_t end;
};

struct Block
{
    Offset first;
    std::uint32_t width;
    std::vector<Instruction> instructions;
    std::vector<std::uint32_t> operands;
    std::vector<double> weights;
    std::vector<Offset> loads;
    std::vector<std::uint32_t> outputs;
    std::vector<Interval> intervals;
    std::vector<Offset> starts;
};

class Tape
{
  public:
    Tape(const Graph &graph, std::span<const Offset> roots, Offset budget = kBudget);

    Offset roots() const noexcept;
    Offset variables() const noexcept;
    Offset pinned() const noexcept;
    std::uint32_t width() const noexcept;
    Offset prepass() const noexcept;
    std::span<const Block> blocks() const noexcept;
    std::span<const Interval> intervals(Offset root) const;

  private:
    std::vector<Block> blocks_;
    Offset roots_ = 0;
    Offset variables_ = 0;
    Offset pinned_ = 0;
    std::uint32_t width_ = 0;
    Offset prepass_ = 0;
};

}
