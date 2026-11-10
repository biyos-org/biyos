#include "allocations.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace
{

std::atomic<std::size_t> count{0};

void *counted(std::size_t size) noexcept
{
    ++count;
    return std::malloc(size == 0 ? 1 : size);
}

void *allocate(std::size_t size)
{
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
