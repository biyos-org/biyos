#pragma once

#include "core/env.hpp"
#include "core/events/events.hpp"
#include "core/text/name.hpp"
#include "core/timing/clock.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace biyos
{

class TimingTree
{
  public:
    struct Node
    {
        std::string_view name;
        std::size_t parent;
        std::chrono::nanoseconds wall;
        std::chrono::nanoseconds cpu;
        std::int64_t count;
        bool open;
    };

    static constexpr std::size_t kNone = (std::numeric_limits<std::size_t>::max)();

    std::span<const Node> nodes() const noexcept { return nodes_; }
    void reserve(std::size_t nodes) { nodes_.reserve(nodes); }

  private:
    friend class Timer;

    std::size_t find(std::string_view name) const noexcept;
    std::size_t add(std::string_view name);
    std::size_t enter(std::string_view name);
    std::size_t open_ancestor(std::size_t node) const noexcept;
    void leave(std::size_t node,
               std::chrono::nanoseconds wall,
               std::chrono::nanoseconds cpu) noexcept;

    std::vector<Node> nodes_;
    std::size_t current_ = kNone;
    std::chrono::nanoseconds observed_wall_{0};
    std::chrono::nanoseconds observed_cpu_{0};
};

class Timer
{
  public:
    Timer(Env &env, TimingTree &tree, StaticName module, StaticName name);
    ~Timer();

    Timer(const Timer &) = delete;
    Timer &operator=(const Timer &) = delete;

  private:
    void announce(std::string_view edge,
                  std::chrono::nanoseconds wall,
                  std::chrono::nanoseconds cpu) noexcept;

    const Clock &clock_;
    Events &events_;
    TimingTree &tree_;
    std::string_view module_;
    std::chrono::nanoseconds wall_;
    std::chrono::nanoseconds cpu_;
    std::chrono::nanoseconds observed_wall_;
    std::chrono::nanoseconds observed_cpu_;
    std::size_t node_;
};

class EntryTimer
{
  public:
    EntryTimer(Env &env, StaticName module, StaticName name) : timer_(env, tree_, module, name) {}

  private:
    TimingTree tree_;
    Timer timer_;
};

}
