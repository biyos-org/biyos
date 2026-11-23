#pragma once

#include "biyos/biyos.h"

#include <array>
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace biyos
{

enum class EventKind
{
    Message = BIYOS_EVT_MESSAGE,
    Check = BIYOS_EVT_CHECK,
    Phase = BIYOS_EVT_PHASE,
};

enum class EventLevel
{
    Debug = BIYOS_LVL_DBG,
    Info = BIYOS_LVL_INFO,
    Warning = BIYOS_LVL_WARN,
    Error = BIYOS_LVL_ERR,
};

inline constexpr std::array kEventKinds{EventKind::Message, EventKind::Check, EventKind::Phase};

EventKind find_kind(int id);
EventLevel find_level(int id);
std::string_view name_of(EventKind kind) noexcept;
std::string_view name_of(EventLevel level) noexcept;

struct Field
{
    std::string name;
    std::string value;
};

struct Event
{
    EventKind kind;
    EventLevel level;
    std::chrono::nanoseconds time;
    std::string module;
    std::string text;
    std::vector<Field> fields;
    const BIYOS_model *model = nullptr;
};

using Hook = std::function<void(const Event &)>;

}
