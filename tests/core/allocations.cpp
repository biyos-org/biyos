#include "allocations.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace
{

std::atomic<std::size_t> count{0};
std::atomic<std::size_t> remaining{0};

void *counted(std::size_t size) noexcept
{
    ++count;
    return std::malloc(size == 0 ? 1 : size);
}

bool due() noexcept
{
    const std::size_t left = remaining.load();
    if (left == 0)
    {
        return false;
    }
    remaining.store(left - 1);
    return left == 1;
}

void *allocate(std::size_t size)
{
    if (due())
    {
        throw std::bad_alloc();
    }
    void *memory = counted(size);
    if (memory == nullptr)
    {
        throw std::bad_alloc();
    }
    return memory;
}

}

std::size_t biyos::test::allocations() noexcept
{
    return count.load();
}

void biyos::test::fail_after(std::size_t successes) noexcept
{
    remaining.store(successes + 1);
}

void biyos::test::stop_failing() noexcept
{
    remaining.store(0);
}

void *operator new(std::size_t size)
{
    return allocate(size);
}

void *operator new[](std::size_t size)
{
    return allocate(size);
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept
{
    return counted(size);
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept
{
    return counted(size);
}

void operator delete(void *memory) noexcept
{
    std::free(memory);
}

void operator delete[](void *memory) noexcept
{
    std::free(memory);
}

void operator delete(void *memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void *memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete(void *memory, const std::nothrow_t &) noexcept
{
    std::free(memory);
}

void operator delete[](void *memory, const std::nothrow_t &) noexcept
{
    std::free(memory);
}
