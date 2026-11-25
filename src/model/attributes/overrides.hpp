#pragma once

#include "core/numbers.hpp"

#include <map>
#include <span>
#include <string_view>

namespace biyos
{

struct Tolerance
{
    double absolute;
    double relative;
};

class Overrides
{
  public:
    Tolerance at(Index index) const noexcept;
    void set(std::string_view noun,
             Index size,
             std::span<const Index> indices,
             std::span<const double> absolute,
             std::span<const double> relative);
    void remove(std::string_view noun, Index size, std::span<const Index> indices);
    void renumber(std::span<const Index> map) noexcept;

  private:
    std::map<Index, Tolerance> held_;
};

}
