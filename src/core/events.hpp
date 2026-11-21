#pragma once

#include "biyos/biyos.h"
#include "core/clock.hpp"

#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
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
std::string_view name_of(EventKind knd) noexcept;
std::string_view name_of(EventLevel lvl) noexcept;

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
};

using Hook = std::function<void(const Event &)>;

class Events
{
  public:
    explicit Events(const Clock &clock);

    int add_hook(EventKind knd, EventLevel lvl, Hook hk);
    void del_hook(int id);

    bool wanted(EventKind knd, EventLevel lvl) const noexcept { return lowest_[slot(knd)] <= lvl; }
    bool delivering() const noexcept { return depth_ > 0; }
    bool filling() const noexcept { return fills_ > 0; }
    std::chrono::nanoseconds origin() const noexcept { return origin_; }

    template <std::invocable<Event &> Fill>
    void emit(EventKind knd, EventLevel lvl, Fill &&fill) noexcept
    {
        if (!wanted(knd, lvl))
        {
            return;
        }
        Event evt{
            .kind = knd,
            .level = lvl,
            .time = clock_.wall_time() - origin_,
            .module = {},
            .text = {},
            .fields = {},
        };
        try
        {
            const Depth inside(fills_);
            fill(evt);
        }
        catch (...)
        {
            return;
        }
        deliver(evt);
    }

  private:
    class Depth
    {
      public:
        explicit Depth(int &n) noexcept : count_(n) { ++count_; }

        ~Depth() { --count_; }

        Depth(const Depth &) = delete;
        Depth &operator=(const Depth &) = delete;

      private:
        int &count_;
    };

    struct Entry
    {
        int id;
        EventKind kind;
        EventLevel lowest;
        std::unique_ptr<const Hook> hook;
        bool active;
    };

    static constexpr EventLevel kNobody = static_cast<EventLevel>(BIYOS_LVL_ERR + 1);

    static std::size_t slot(EventKind knd) noexcept { return static_cast<std::size_t>(knd) - 1; }

    static bool accepts(const Entry &entry, EventKind knd, EventLevel lvl) noexcept;

    void refresh(EventKind knd) noexcept;
    void deliver(const Event &evt) noexcept;
    void prune() noexcept;

    const Clock &clock_;
    std::chrono::nanoseconds origin_;
    std::deque<Entry> hooks_;
    std::array<EventLevel, kEventKinds.size()> lowest_;
    int next_id_ = 1;
    int depth_ = 0;
    int fills_ = 0;
};

}
