#pragma once

#include <cstddef>

namespace biyos::test
{

// Calls so far to the global operator new and new[], plain and nothrow, which allocations.cpp
// replaces with their deletes by malloc and free. That hides a sanitizer's new and delete checks,
// so only the allocation test program links it. Aligned forms are neither replaced nor counted.
std::size_t allocations() noexcept;

}
