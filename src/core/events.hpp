#pragma once

#include "biyos/events.h"

#include <concepts>
#include <deque>
#include <functional>
#include <string>
#include <vector>

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

struct Field
{
    std::string name;
    std::string value;
};

struct Event
{
    EventKind kind;
    Level level;
    std::string module;
    std::string text;
    std::vector<Field> fields;
};

using Subscriber = std::function<void(const Event &)>;

// Delivery follows subscription order. Subscribing or unsubscribing during delivery is allowed:
// the deque never moves a running subscriber and removal waits until delivery ends.
class Events
{
  public:
    int subscribe(EventKind kind, Level lowest, Subscriber subscriber);
    void unsubscribe(int id);

    bool wanted(EventKind kind, Level level) const noexcept;

    template <std::invocable<Event &> Fill> void emit(EventKind kind, Level level, Fill &&fill)
    {
        if (!wanted(kind, level))
        {
            return;
        }
        Event event{.kind = kind, .level = level, .module = {}, .text = {}, .fields = {}};
        fill(event);
        deliver(event);
    }

  private:
    struct Subscription
    {
        int id;
        EventKind kind;
        Level lowest;
        Subscriber subscriber;
        bool active;
    };

    static bool accepts(const Subscription &subscription, EventKind kind, Level level) noexcept;

    void deliver(const Event &event);

    std::deque<Subscription> subscriptions_;
    int next_id_ = 1;
    int depth_ = 0;
};

}
