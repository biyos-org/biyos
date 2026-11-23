#pragma once

#include "core/index.hpp"

#include <map>
#include <span>
#include <string_view>

namespace biyos
{

struct Override
{
    double absolute;
    double relative;
};

class Overrides
{
  public:
    Override at(Index index) const noexcept;
    void set(std::string_view noun,
             Index size,
             std::span<const Index> indices,
             std::span<const double> absolute,
             std::span<const double> relative);
    void remove(std::string_view noun, Index size, std::span<const Index> indices);

  private:
    std::map<Index, Override> held_;
};

}
