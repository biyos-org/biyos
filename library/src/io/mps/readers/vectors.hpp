#pragma once

#include "core/events/stream.hpp"
#include "io/utils/lines.hpp"

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

    bool keeps(Stream stream, const Lines &lines, std::string_view name);

  private:
    std::string section_;
    std::optional<std::string> kept_;
    std::set<std::string, std::less<>> skipped_;
};

}
