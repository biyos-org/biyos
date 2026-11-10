#pragma once

#include <compare>

namespace biyos
{

struct Version
{
    int major;
    int minor;
    int patch;

    auto operator<=>(const Version &) const = default;
};

Version version();

}
