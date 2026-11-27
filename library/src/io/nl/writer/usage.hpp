#pragma once

#include "core/numbers.hpp"
#include "model/model.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace biyos::nl
{

inline constexpr Index kNobody = -1;
inline constexpr Index kSeveral = -2;

struct Reach
{
    Index constraint;
    Index objective;
};

inline constexpr Reach kUnreached{.constraint = kNobody, .objective = kNobody};

bool reached(const Reach &reach) noexcept;

class Usage
{
  public:
    Usage(const Model &model, std::span<const Index> constraints);

    bool used(Offset node) const noexcept;
    bool shared(Offset node) const noexcept;
    const Reach &node_reach(Offset node) const noexcept;
    const Reach &function_reach(Index function) const noexcept;
    const Reach &variable_reach(Index variable) const noexcept;

  private:
    void reach_functions(const Model &model, std::span<const Index> constraints);
    void reach_roots(const Model &model);
    void reach_nodes(const Graph &graph);
    void reach_variables(const Model &model);
    void pass(Offset node, const Reach &reach, Index owner);

    std::vector<Reach> functions_;
    std::vector<Reach> nodes_;
    std::vector<std::uint8_t> counts_;
    std::vector<Index> owners_;
    std::vector<Reach> variables_;
};

}
