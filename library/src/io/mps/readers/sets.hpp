#pragma once

#include "core/numbers.hpp"
#include "io/draft/draft.hpp"
#include "io/lines.hpp"
#include "model/model.hpp"

#include <optional>
#include <set>
#include <string_view>

namespace biyos
{

std::optional<SpecialOrderedSetKind> set_kind(std::string_view code) noexcept;
std::string_view code_of(SpecialOrderedSetKind kind) noexcept;

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
