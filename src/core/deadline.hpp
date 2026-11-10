#pragma once

#include "core/clock.hpp"
#include "core/options.hpp"

#include <chrono>
#include <optional>

namespace biyos
{

// The time limits of one solve, read once when it starts. A limit is its seconds times 1e9 in
// double, rounded up to whole nanoseconds, since elapsed time is whole. A product of 2^63 or more,
// infinity included, is no limit, and its clock is never read.
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
