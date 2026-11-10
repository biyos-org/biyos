#pragma once

#include <cstddef>
#include <string_view>

namespace biyos
{

// A name fixed at compile time, such as a phase or a module. The constructor is consteval and
// reads the text, so only a constant array binds: a local or mutable buffer does not compile, and
// the view never dangles.
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
