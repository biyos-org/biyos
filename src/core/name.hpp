#pragma once

#include <cstddef>
#include <string_view>

namespace biyos
{

class FixedName
{
  public:
    template <std::size_t Size> consteval FixedName(const char (&text)[Size]) noexcept : text_(text)
    {
    }

    constexpr std::string_view text() const noexcept { return text_; }

  private:
    std::string_view text_;
};

}
