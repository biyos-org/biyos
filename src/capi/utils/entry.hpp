#pragma once

#include "biyos/biyos.h"
#include "capi/utils/busy.hpp"
#include "capi/utils/firewall.hpp"
#include "capi/utils/types.hpp"
#include "core/env.hpp"
#include "core/error.hpp"
#include "core/events/events.hpp"
#include "model/model.hpp"
#include "model/session/session.hpp"

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>

struct BIYOS_env
{
    biyos::Env core;
};

struct BIYOS_model final : biyos::Session
{
    using Session::Session;
};

namespace biyos::capi
{

inline const BIYOS_model *handle_of(const Session *session) noexcept
{
    return static_cast<const BIYOS_model *>(session);
}

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
    EnvGuard::claim(idle(env, "environment"));
    return env;
}

inline void claim(BIYOS_model &model)
{
    if (model.env().events().delivering())
    {
        const std::string reason = "is read-only while delivering an event";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, "model", reason);
    }
    ModelGuard::claim(model.usage());
    if (model.evaluating())
    {
        refuse("model", kRunning);
    }
}

inline biyos::Model &writable(BIYOS_model &model)
{
    claim(model);
    return model.change();
}

inline void start(biyos::Env &env) noexcept
{
    if (!env.events().delivering())
    {
        env.interrupt().clear();
    }
}

template <auto Body, typename Guard, typename Target, typename... Args>
void admitted(const Guard &busy, Target &target, Args... arguments)
{
    busy.check();
    Body(target, arguments...);
}

template <auto Body, typename... Args>
BIYOS_status guarded(BIYOS_env *env, Args... arguments) noexcept
{
    if (env == nullptr)
    {
        return BIYOS_ERR_BADARG;
    }
    biyos::Env &core = env->core;
    start(core);
    const EnvGuard busy(core);
    return biyos::capi::guard(&core,
                              nullptr,
                              nullptr,
                              [&] { admitted<Body>(busy, core, arguments...); });
}

template <auto Body, typename Model, typename... Args>
    requires std::same_as<std::remove_const_t<Model>, BIYOS_model>
BIYOS_status guarded(Model *model, Args... arguments) noexcept
{
    if (model == nullptr)
    {
        return BIYOS_ERR_BADARG;
    }
    biyos::Env &env = model->env();
    if (!model->usage().held())
    {
        start(env);
    }
    const ModelGuard busy(env, model->usage());
    return biyos::capi::guard(&env,
                              model,
                              &model->hooks(),
                              [&] { admitted<Body>(busy, *model, arguments...); });
}

}
