#include "core/timing/deadline.hpp"

#include <chrono>
#include <optional>
#include <ratio>

namespace biyos
{

namespace
{

using std::chrono::nanoseconds;

constexpr std::chrono::duration<double, std::nano> kLongest = (nanoseconds::max)();

std::optional<nanoseconds> limit_of(const Lookup &lookup, Option option)
{
    const std::chrono::duration<double> limit(lookup.get_double(option));
    if (limit < kLongest)
    {
        return std::chrono::ceil<nanoseconds>(limit);
    }
    return std::nullopt;
}

}

Deadline::Deadline(const Lookup &lookup, const Clock &clock, const Interrupt &interrupt)
    : clock_(clock), interrupt_(interrupt), seen_(interrupt.count()),
      wall_limit_(limit_of(lookup, Option::TimeLimit)),
      cpu_limit_(limit_of(lookup, Option::CpuTimeLimit)),
      wall_start_(wall_limit_ ? clock.wall_time() : nanoseconds::zero()),
      cpu_start_(cpu_limit_ ? clock.cpu_time() : nanoseconds::zero())
{
}

bool Deadline::reached() const
{
    return termination() != TerminationStatus::NotSolved;
}

TerminationStatus Deadline::termination() const
{
    if (interrupt_.count() != seen_)
    {
        return TerminationStatus::UserStop;
    }
    if (wall_reached())
    {
        return TerminationStatus::TimeLimit;
    }
    if (cpu_reached())
    {
        return TerminationStatus::CpuTimeLimit;
    }
    return TerminationStatus::NotSolved;
}

bool Deadline::wall_reached() const
{
    return wall_limit_ && clock_.wall_time() - wall_start_ >= *wall_limit_;
}

bool Deadline::cpu_reached() const
{
    return cpu_limit_ && clock_.cpu_time() - cpu_start_ >= *cpu_limit_;
}

}
