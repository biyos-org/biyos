#pragma once

#include "biyos/biyos.h"
#include "core/event.hpp"
#include "core/thread.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>

namespace biyos
{

class Hooks
{
  public:
    Hooks();
    Hooks(const Hooks &) = delete;
    Hooks &operator=(const Hooks &) = delete;

    int add_hook(EventKind kind, EventLevel level, Hook hook);
    void delete_hook(int id);

    bool wanted(EventKind kind, EventLevel level) const noexcept
    {
        return lowest_[slot(kind)].load() <= level;
    }
    bool delivering() const noexcept { return mine() && depth_ > 0; }

    void deliver(const Event &event) noexcept;

  private:
    class Turn
    {
      public:
        explicit Turn(Hooks &hooks) noexcept;
        ~Turn();
        Turn(const Turn &) = delete;
        Turn &operator=(const Turn &) = delete;

      private:
        Hooks &hooks_;
        bool nested_;
    };

    struct Entry
    {
        int id;
        EventKind kind;
        EventLevel lowest;
        std::unique_ptr<const Hook> hook;
        bool active;
    };

    static constexpr EventLevel kNobody = static_cast<EventLevel>(BIYOS_EVTLVL_ERR + 1);

    static std::size_t slot(EventKind kind) noexcept { return static_cast<std::size_t>(kind) - 1; }

    bool mine() const noexcept { return owner_.load() == this_thread(); }

    static bool accepts(const Entry &entry, EventKind kind, EventLevel level) noexcept;

    void refresh(EventKind kind) noexcept;
    void prune() noexcept;

    std::deque<Entry> entries_;
    std::array<std::atomic<EventLevel>, kEventKinds.size()> lowest_;
    int next_id_ = 1;
    int depth_ = 0;
    std::mutex turn_;
    std::atomic<ThreadId> owner_;
};

}
