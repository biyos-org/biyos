#pragma once

#include "core/env.hpp"
#include "io/lines.hpp"

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace biyos
{

class Vectors
{
  public:
    explicit Vectors(std::string_view section);

    bool keeps(Env &env, const Lines &lines, std::string_view name);

  private:
    std::string section_;
    std::optional<std::string> kept_;
    std::set<std::string, std::less<>> skipped_;
};

}
