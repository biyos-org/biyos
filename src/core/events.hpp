#pragma once

#include "biyos/events.h"

namespace biyos
{

enum class EventKind
{
    Message = BIYOS_EVENT_MESSAGE,
};

enum class Level
{
    Debug = BIYOS_LEVEL_DEBUG,
    Info = BIYOS_LEVEL_INFO,
    Warning = BIYOS_LEVEL_WARNING,
    Error = BIYOS_LEVEL_ERROR,
};

EventKind find_kind(int id);
Level find_level(int id);

}
