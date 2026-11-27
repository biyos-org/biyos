#pragma once

#include "core/numbers.hpp"
#include "io/draft/draft.hpp"
#include "io/utils/lines.hpp"
#include "model/model.hpp"

#include <set>

namespace biyos
{

class Sets
{
  public:
    void read(const Lines &lines, Draft &draft);

  private:
    void open(const Lines &lines, Draft &draft, SpecialOrderedSetKind kind);
    void add(const Lines &lines, Draft &draft);

    std::set<Index> columns_;
    std::set<Real> weights_;
};

}
