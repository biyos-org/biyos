#pragma once

#include "core/numbers.hpp"
#include "expr/graph/graph.hpp"
#include "io/nl/writer/usage.hpp"
#include "model/model.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace biyos::nl
{

enum class Scope
{
    Both,
    Constraints,
    Objectives,
    Constraint,
    Objective,
};

inline constexpr std::size_t kScopes = 5;

struct Definition
{
    Offset node;
    Scope scope;
    Index record;
};

bool direct(const Graph &graph, Offset node) noexcept;
Index lead_of(const Graph &graph, Offset node) noexcept;

class Defined
{
  public:
    Defined(const Model &model, const Usage &usage);

    bool has(Offset node) const noexcept;
    Index index_of(Offset node) const noexcept;
    Index link_of(const Definition &definition) const noexcept;
    std::span<const Definition> common() const noexcept;
    std::span<const Definition> of(Scope scope, Index record) const noexcept;
    std::array<Index, kScopes> counts() const noexcept;

  private:
    Index constraints_;
    std::vector<Definition> definitions_;
    std::vector<Index> indices_;
};

}
