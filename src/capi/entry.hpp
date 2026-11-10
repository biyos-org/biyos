#pragma once

#include "biyos/biyos.h"
#include "capi/firewall.hpp"
#include "core/environment.hpp"
#include "core/error.hpp"
#include "core/events.hpp"

#include <string>
#include <string_view>

struct biyos_env
{
    biyos::Environment core;
};

namespace biyos::capi
{

template <typename T> T *require(T *pointer, std::string_view name)
{
    if (pointer == nullptr)
    {
        throw biyos::Error(biyos::ErrorKind::InvalidArgument,
                           "argument '" + std::string(name) + "'", "is null");
    }
    return pointer;
}

inline biyos::Environment &writable(biyos::Environment &env)
{
    if (env.events().delivering())
    {
        const std::string reason = "is read-only while delivering an event";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, "environment", reason);
    }
    return env;
}

template <auto Body, typename... Args> biyos_status guarded(biyos_env *env, Args... args) noexcept
{
    if (env == nullptr)
    {
        return BIYOS_INVALID_ARGUMENT;
    }
    return biyos::capi::guard(&env->core, [&] { Body(env->core, args...); });
}

}
