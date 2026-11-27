#pragma once

#include "core/env/thread.hpp"

#include <atomic>
#include <mutex>

namespace biyos
{

class ErrorScope;

class Deliveries
{
  public:
    struct State
    {
        int depth = 0;
        int fills = 0;
        ErrorScope *scope = nullptr;
    };

    class Entry
    {
      public:
        explicit Entry(Deliveries &deliveries) noexcept;
        ~Entry();
        Entry(const Entry &) = delete;
        Entry &operator=(const Entry &) = delete;

        State &state() const noexcept { return *state_; }

      private:
        friend class Deliveries;

        Deliveries &deliveries_;
        ThreadId thread_;
        State own_;
        State *state_;
        Entry *next_ = nullptr;
    };

    Deliveries() = default;
    Deliveries(const Deliveries &) = delete;
    Deliveries &operator=(const Deliveries &) = delete;

    bool delivering() const noexcept;
    bool filling() const noexcept;
    State *delivery() noexcept;

  private:
    State *find() const noexcept;
    Entry *entry_of(ThreadId thread) const noexcept;

    std::atomic<int> count_{0};
    mutable std::mutex lock_;
    Entry *first_ = nullptr;
};

}
