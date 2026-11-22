#pragma once

#include "biyos/biyos.h"
#include "capi/firewall.hpp"
#include "core/env.hpp"
#include "core/error.hpp"
#include "core/events.hpp"
#include "model/model.hpp"

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>

struct BIYOS_env
{
    biyos::Env core;
};

struct BIYOS_model
{
    explicit BIYOS_model(biyos::Env &env) : handle(env) {}
    BIYOS_model(biyos::Env &env, const biyos::Model &model) : handle(env), core(model) {}

    biyos::Env::Handle handle;
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

inline biyos::Env &idle(biyos::Env &env, std::string_view object)
{
    if (env.events().delivering())
    {
        const std::string reason = "is read-only while delivering an event";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, std::string(object), reason);
    }
    return env;
}

inline biyos::Env &writable(biyos::Env &env)
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

inline void start(biyos::Env &env) noexcept
{
    if (!env.events().delivering())
    {
        env.interrupt().clear();
    }
}

template <auto Body, typename... Args>
BIYOS_status guarded(BIYOS_env *env, Args... arguments) noexcept
{
    if (env == nullptr)
    {
        return BIYOS_ERR_BADARG;
    }
    start(env->core);
    return biyos::capi::guard(&env->core, [&] { Body(env->core, arguments...); });
}

template <auto Body, typename Model, typename... Args>
    requires std::same_as<std::remove_const_t<Model>, BIYOS_model>
BIYOS_status guarded(Model *model, Args... arguments) noexcept
{
    if (model == nullptr)
    {
        return BIYOS_ERR_BADARG;
    }
    start(model->handle.env());
    return biyos::capi::guard(&model->handle.env(), [&] { Body(*model, arguments...); });
}

}
