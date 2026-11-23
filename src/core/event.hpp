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
    Message = BIYOS_EVTKND_MSG,
    Invariant = BIYOS_EVTKND_INV,
    Span = BIYOS_EVTKND_SPAN,
};

enum class EventLevel
{
    Debug = BIYOS_EVTLVL_DBG,
    Info = BIYOS_EVTLVL_INFO,
    Warning = BIYOS_EVTLVL_WARN,
    Error = BIYOS_EVTLVL_ERR,
};

inline constexpr std::array kEventKinds{EventKind::Message, EventKind::Invariant, EventKind::Span};

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
