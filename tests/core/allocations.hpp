#pragma once

#include <cstddef>

namespace biyos::test
{

std::size_t allocations() noexcept;
void fail_after(std::size_t successes) noexcept;
void stop_failing() noexcept;

}
