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
    Timer(Environment &env, TimingTree &tree, FixedName module, FixedName name);
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

}
