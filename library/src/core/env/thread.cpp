#include "core/env/thread.hpp"

#include <thread>

namespace biyos
{

ThreadId this_thread() noexcept
{
    return std::this_thread::get_id();
}

}
