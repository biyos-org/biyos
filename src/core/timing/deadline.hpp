#pragma once

#include "core/options/lookup.hpp"
#include "core/status.hpp"
#include "core/timing/clock.hpp"
#include "core/timing/interrupt.hpp"

#include <chrono>
#include <optional>

namespace biyos
{

class Deadline
{
  public:
    Deadline(const Lookup &lookup, const Clock &clock, const Interrupt &interrupt);

    bool reached() const;
    TerminationStatus termination() const;

  private:
    bool wall_reached() const;
    bool cpu_reached() const;

    const Clock &clock_;
    const Interrupt &interrupt_;
    unsigned seen_;
    std::optional<std::chrono::nanoseconds> wall_limit_;
    std::optional<std::chrono::nanoseconds> cpu_limit_;
    std::chrono::nanoseconds wall_start_;
    std::chrono::nanoseconds cpu_start_;
};

}
