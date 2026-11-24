#include "core/events/event.hpp"

#include "core/error.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace biyos
{

namespace
{

constexpr std::array kLevels{EventLevel::Debug,
                             EventLevel::Info,
                             EventLevel::Warning,
                             EventLevel::Error};

constexpr bool numbered()
{
    for (std::size_t i = 0; i < kEventKinds.size(); ++i)
    {
        if (static_cast<std::size_t>(kEventKinds[i]) != i + 1)
        {
            return false;
        }
    }
    return true;
}

static_assert(numbered());
static_assert(std::ranges::is_sorted(kLevels));

}

EventKind find_kind(int id)
{
    const auto kind = static_cast<EventKind>(id);
    if (std::ranges::find(kEventKinds, kind) == kEventKinds.end())
    {
        throw Error(ErrorKind::InvalidArgument, "event kind " + std::to_string(id), "unknown");
    }
    return kind;
}

EventLevel find_level(int id)
{
    const auto level = static_cast<EventLevel>(id);
    if (std::ranges::find(kLevels, level) == kLevels.end())
    {
        throw Error(ErrorKind::InvalidArgument, "level " + std::to_string(id), "unknown");
    }
    return level;
}

std::string_view name_of(EventKind kind) noexcept
{
    switch (kind)
    {
    case EventKind::Message:
        return "message";
    case EventKind::Invariant:
        return "invariant";
    case EventKind::Span:
        return "span";
    }
    return "";
}

std::string_view name_of(EventLevel level) noexcept
{
    switch (level)
    {
    case EventLevel::Debug:
        return "debug";
    case EventLevel::Info:
        return "info";
    case EventLevel::Warning:
        return "warning";
    case EventLevel::Error:
        return "error";
    }
    return "";
}

}
