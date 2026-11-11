#pragma once

#include "biyos/biyos.h"
#include "capi/firewall.hpp"
#include "core/environment.hpp"
#include "core/error.hpp"
#include "core/events.hpp"
#include "model/model.hpp"

#include <string>
#include <string_view>

struct biyos_env
{
    biyos::Environment core;
};

struct biyos_model
{
    explicit biyos_model(biyos::Environment &env) : handle(env) {}

    biyos::Environment::Handle handle;
    biyos::Model core;
};

namespace biyos::capi
{

template <typename T> T *require(T *pointer, std::string_view name)
{
    if (pointer == nullptr)
    {
        throw biyos::Error(biyos::ErrorKind::InvalidArgument,
                           "argument '" + std::string(name) + "'",
                           "is null");
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

inline biyos::Model &writable(biyos_model &model)
{
    if (model.handle.env().events().delivering())
    {
        const std::string reason = "is read-only while delivering an event";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, "model", reason);
    }
    return model.core;
}

template <auto Body, typename... Args> biyos_status guarded(biyos_env *env, Args... args) noexcept
{
    if (env == nullptr)
    {
        return BIYOS_INVALID_ARGUMENT;
    }
    return biyos::capi::guard(&env->core, [&] { Body(env->core, args...); });
}

template <auto Body, typename... Args>
biyos_status guarded(biyos_model *model, Args... args) noexcept
{
    if (model == nullptr)
    {
        return BIYOS_INVALID_ARGUMENT;
    }
    return biyos::capi::guard(&model->handle.env(), [&] { Body(*model, args...); });
}

}
