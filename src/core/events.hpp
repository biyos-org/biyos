#pragma once

#include "biyos/events.h"
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
    Message = BIYOS_EVENT_MESSAGE,
    Check = BIYOS_EVENT_CHECK,
    Phase = BIYOS_EVENT_PHASE,
};

enum class Level
{
    Debug = BIYOS_LEVEL_DEBUG,
    Info = BIYOS_LEVEL_INFO,
    Warning = BIYOS_LEVEL_WARNING,
    Error = BIYOS_LEVEL_ERROR,
};

inline constexpr std::array kEventKinds{EventKind::Message, EventKind::Check, EventKind::Phase};

EventKind find_kind(int id);
Level find_level(int id);
std::string_view name_of(EventKind kind) noexcept;
std::string_view name_of(Level level) noexcept;

struct Field
{
    std::string name;
    std::string value;
};

struct Event
{
    EventKind kind;
    Level level;
    std::chrono::nanoseconds time;
    std::string module;
    std::string text;
    std::vector<Field> fields;
};

using Subscriber = std::function<void(const Event &)>;

class Events
{
  public:
    explicit Events(const Clock &clock);

    int subscribe(EventKind kind, Level lowest, Subscriber subscriber);
    void unsubscribe(int id);

    bool wanted(EventKind kind, Level level) const noexcept { return lowest_[slot(kind)] <= level; }
    bool delivering() const noexcept { return depth_ > 0; }
    bool filling() const noexcept { return fills_ > 0; }
    std::chrono::nanoseconds origin() const noexcept { return origin_; }

    template <std::invocable<Event &> Fill>
    void emit(EventKind kind, Level level, Fill &&fill) noexcept
    {
        if (!wanted(kind, level))
        {
            return;
        }
        Event event{
            .kind = kind,
            .level = level,
            .time = clock_.wall_time() - origin_,
            .module = {},
            .text = {},
            .fields = {},
        };
        try
        {
            const Depth inside(fills_);
            fill(event);
        }
        catch (...)
        {
            return;
        }
        deliver(event);
    }

  private:
    class Depth
    {
      public:
        explicit Depth(int &count) noexcept : count_(count) { ++count_; }

        ~Depth() { --count_; }

        Depth(const Depth &) = delete;
        Depth &operator=(const Depth &) = delete;

      private:
        int &count_;
    };

    struct Subscription
    {
        int id;
        EventKind kind;
        Level lowest;
        std::unique_ptr<const Subscriber> subscriber;
        bool active;
    };

    static constexpr Level kNobody = static_cast<Level>(BIYOS_LEVEL_ERROR + 1);

    static std::size_t slot(EventKind kind) noexcept { return static_cast<std::size_t>(kind) - 1; }

    static bool accepts(const Subscription &subscription, EventKind kind, Level level) noexcept;

    void refresh(EventKind kind) noexcept;
    void deliver(const Event &event) noexcept;
    void prune() noexcept;

    const Clock &clock_;
    std::chrono::nanoseconds origin_;
    std::deque<Subscription> subscriptions_;
    std::array<Level, kEventKinds.size()> lowest_;
    int next_id_ = 1;
    int depth_ = 0;
    int fills_ = 0;
};

}
