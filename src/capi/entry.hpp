#pragma once

#include "biyos/biyos.h"
#include "capi/firewall.hpp"
#include "core/environment.hpp"
#include "core/error.hpp"
#include "core/events.hpp"
#include "model/model.hpp"

#include <string>
#include <string_view>

struct BIYOS_env
{
    biyos::Environment core;
};

struct BIYOS_model
{
    explicit BIYOS_model(biyos::Environment &env) : handle(env) {}
    BIYOS_model(biyos::Environment &env, const biyos::Model &model) : handle(env), core(model) {}

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

inline biyos::Environment &idle(biyos::Environment &env, std::string_view object)
{
    if (env.events().delivering())
    {
        const std::string reason = "is read-only while delivering an event";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, std::string(object), reason);
    }
    return env;
}

inline biyos::Environment &writable(biyos::Environment &env)
{
    return idle(env, "environment");
}

inline biyos::Model &writable(BIYOS_model &model)
{
    if (model.handle.env().events().delivering())
    {
        const std::string reason = "is read-only while delivering an event";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, "model", reason);
    }
    return model.core;
}

template <auto Body, typename... Args> BIYOS_status guarded(BIYOS_env *env, Args... args) noexcept
{
    if (env == nullptr)
    {
        return BIYOS_ERR_INVALIDARG;
    }
    return biyos::capi::guard(&env->core, [&] { Body(env->core, args...); });
}

template <auto Body, typename... Args>
BIYOS_status guarded(BIYOS_model *model, Args... args) noexcept
{
    if (model == nullptr)
    {
        return BIYOS_ERR_INVALIDARG;
    }
    return biyos::capi::guard(&model->handle.env(), [&] { Body(*model, args...); });
}

}
