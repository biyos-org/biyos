#pragma once

#include <thread>

namespace biyos
{

using ThreadId = std::thread::id;

ThreadId this_thread() noexcept;

}
