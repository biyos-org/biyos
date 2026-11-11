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

constexpr int kVersionBase = 100;

Version version();

constexpr bool numbered(Version version)
{
    const auto fits = [](int part) { return 0 <= part && part < kVersionBase; };
    return 0 <= version.major && fits(version.minor) && fits(version.patch);
}

constexpr int number_of(Version version)
{
    return (version.major * kVersionBase + version.minor) * kVersionBase + version.patch;
}

}
