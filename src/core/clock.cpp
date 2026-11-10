#include "core/clock.hpp"

#include "core/error.hpp"

#include <chrono>

#if defined(_WIN32)
#include <cstdint>
#include <ratio>
#include <windows.h>
#else
#include <time.h>
#endif

namespace biyos
{

namespace
{

Error unreadable()
{
    return Error(ErrorKind::Internal, "clock", "cannot read the CPU time");
}

#if defined(_WIN32)

using Ticks = std::chrono::duration<std::int64_t, std::ratio<100, 1'000'000'000>>;

Ticks ticks_of(const FILETIME &value) noexcept
{
    const std::uint64_t high = value.dwHighDateTime;
    const std::uint64_t count = (high << 32U) | value.dwLowDateTime;
    return Ticks(static_cast<std::int64_t>(count));
}

std::chrono::nanoseconds process_time()
{
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) == 0)
    {
        throw unreadable();
    }
    return ticks_of(kernel) + ticks_of(user);
}

#else

std::chrono::nanoseconds process_time()
{
    timespec now{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now) != 0)
    {
        throw unreadable();
    }
    return std::chrono::seconds(now.tv_sec) + std::chrono::nanoseconds(now.tv_nsec);
}

#endif

}

std::chrono::nanoseconds SystemClock::wall_time() const noexcept
{
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
}

std::chrono::nanoseconds SystemClock::cpu_time() const
{
    return process_time();
}

}
