#pragma once

#include "core/clock.hpp"
#include "core/options.hpp"

#include <chrono>
#include <optional>

namespace biyos
{

class Deadline
{
  public:
    Deadline(const Options &options, const Clock &clock);

    bool reached() const;

  private:
    bool wall_reached() const;
    bool cpu_reached() const;

    const Clock &clock_;
    std::optional<std::chrono::nanoseconds> wall_limit_;
    std::optional<std::chrono::nanoseconds> cpu_limit_;
    std::chrono::nanoseconds wall_start_;
    std::chrono::nanoseconds cpu_start_;
};

}
