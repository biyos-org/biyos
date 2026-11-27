#pragma once

#include <functional>
#include <set>
#include <string>
#include <string_view>

namespace biyos
{

class Strings
{
  public:
    Strings() = default;
    Strings(const Strings &) = delete;
    Strings &operator=(const Strings &) = delete;
    Strings(Strings &&) noexcept = default;
    Strings &operator=(Strings &&) noexcept = default;

    std::string_view keep(std::string_view text);

  private:
    std::set<std::string, std::less<>> kept_;
};

}
