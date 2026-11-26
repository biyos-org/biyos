#include "core/text/strings.hpp"

#include <string>
#include <string_view>

namespace biyos
{

std::string_view Strings::keep(std::string_view text)
{
    const auto held = kept_.find(text);
    if (held != kept_.end())
    {
        return *held;
    }
    return *kept_.emplace(text).first;
}

}
