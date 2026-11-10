#pragma once

#include "core/clock.hpp"
#include "core/environment.hpp"
#include "core/events.hpp"
#include "core/name.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace biyos
{

// Time by phase, nested as Timer scopes nest, with a phase event from the timer's module as each
// scope begins and ends. A node is keyed by parent and name, so a phase entered again under one
// parent adds its wall time, CPU time and count to the same node. In the tree only a node's first
// entry allocates; a phase subscriber's events allocate as every event does. A Timer reads both
// clocks before entering and throws if the CPU time is unreadable, leaving the tree unchanged. On
// leaving, an unreadable CPU time counts as none, since a destructor cannot throw.
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
    };

    static constexpr std::size_t kNone = (std::numeric_limits<std::size_t>::max)();

    std::span<const Node> nodes() const noexcept { return nodes_; }

  private:
    friend class Timer;

    std::size_t find(std::string_view name) const noexcept;
    std::size_t add(std::string_view name);
    std::size_t enter(std::string_view name);
    void leave(std::size_t node, std::chrono::nanoseconds wall,
               std::chrono::nanoseconds cpu) noexcept;

    std::vector<Node> nodes_;
    std::size_t current_ = kNone;
};

class Timer
{
  public:
    Timer(Environment &env, TimingTree &tree, FixedName module, FixedName name);
    ~Timer();

    Timer(const Timer &) = delete;
    Timer &operator=(const Timer &) = delete;

  private:
    void announce(std::string_view edge, std::chrono::nanoseconds wall,
                  std::chrono::nanoseconds cpu) noexcept;

    const Clock &clock_;
    Events &events_;
    TimingTree &tree_;
    std::string_view module_;
    std::chrono::nanoseconds wall_;
    std::chrono::nanoseconds cpu_;
    std::size_t node_;
};

}
