#pragma once

#include "core/numbers.hpp"
#include "expr/forms/terms.hpp"
#include "io/nl/writer/usage.hpp"
#include "model/model.hpp"

#include <span>
#include <vector>

namespace biyos::nl
{

class Entries
{
  public:
    Entries(const Model &model, const Usage &usage, std::span<const Index> position);

    std::span<const LinearTerm> of(Index function) const noexcept;

  private:
    void collect(const Model &model, Index function, std::span<const Index> position);
    void visit(const Graph &graph, Offset root, Index stamp, std::span<const Index> position);
    void add_variable(Index variable, Index stamp, std::span<const Index> position);

    std::vector<Offset> starts_;
    std::vector<LinearTerm> terms_;
    std::vector<Index> seen_;
    std::vector<Index> held_;
    std::vector<Offset> stack_;
};

}
