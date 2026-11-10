#pragma once

#include <chrono>

namespace biyos
{

// Wall time is monotonic. CPU time is the process's, every thread included. Each counts from an
// origin of the clock's choosing, so only differences mean anything.
class Clock
{
  public:
    virtual ~Clock() = default;

    virtual std::chrono::nanoseconds wall_time() const noexcept = 0;
    virtual std::chrono::nanoseconds cpu_time() const = 0;
};

class SystemClock final : public Clock
{
  public:
    std::chrono::nanoseconds wall_time() const noexcept override;
    std::chrono::nanoseconds cpu_time() const override;
};

class FakeClock final : public Clock
{
  public:
    std::chrono::nanoseconds wall_time() const noexcept override { return wall_; }
    std::chrono::nanoseconds cpu_time() const noexcept override { return cpu_; }

    void advance_wall(std::chrono::nanoseconds by) noexcept { wall_ += by; }
    void advance_cpu(std::chrono::nanoseconds by) noexcept { cpu_ += by; }

  private:
    std::chrono::nanoseconds wall_{0};
    std::chrono::nanoseconds cpu_{0};
};

}
