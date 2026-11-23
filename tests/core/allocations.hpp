#pragma once

#include <cstddef>

namespace biyos::test
{

std::size_t allocations() noexcept;
std::size_t allocated_bytes() noexcept;
void fail_after(std::size_t successes) noexcept;
void stop_failing() noexcept;

}
