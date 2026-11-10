#include "core/events.hpp"

#include "core/error.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace biyos
{

namespace
{

constexpr std::array kKinds{EventKind::Message};
constexpr std::array kLevels{Level::Debug, Level::Info, Level::Warning, Level::Error};

static_assert(std::ranges::is_sorted(kLevels));

}

EventKind find_kind(int id)
{
    const auto kind = static_cast<EventKind>(id);
    if (std::ranges::find(kKinds, kind) == kKinds.end())
    {
        throw Error(ErrorKind::InvalidArgument, "event kind " + std::to_string(id), "unknown");
    }
    return kind;
}

Level find_level(int id)
{
    const auto level = static_cast<Level>(id);
    if (std::ranges::find(kLevels, level) == kLevels.end())
    {
        throw Error(ErrorKind::InvalidArgument, "level " + std::to_string(id), "unknown");
    }
    return level;
}

}
