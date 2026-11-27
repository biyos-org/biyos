#pragma once

#include "core/env/env.hpp"
#include "core/events/events.hpp"
#include "core/events/stream.hpp"

#include <functional>

namespace biyos::test
{

inline void quiet(Event &) {}

inline void in_delivery(Env &env, const std::function<void(Env &)> &body)
{
    const auto run = [&env, &body](const Event &) { body(env); };
    const int id = env.events().add_hook(EventKind::Message, EventLevel::Debug, run);
    env.stream().emit(EventKind::Message, EventLevel::Debug, quiet);
    env.events().delete_hook(id);
}

}
