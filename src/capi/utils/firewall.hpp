#pragma once

#include "biyos/biyos.h"
#include "core/env.hpp"
#include "core/error.hpp"
#include "core/events/events.hpp"
#include "core/events/hooks.hpp"

#include <concepts>
#include <exception>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace biyos::capi
{

inline void describe_failure(Event &event,
                             const BIYOS_model *model,
                             BIYOS_status status,
                             std::string_view text)
{
    event.model = model;
    event.module = "capi";
    event.text.assign(text);
    event.fields.push_back(Field{.name = "code", .value = std::to_string(status)});
}

inline void announce(Env &env,
                     const BIYOS_model *model,
                     Hooks *hooks,
                     ErrorKind kind,
                     std::string_view text) noexcept
{
    const BIYOS_status status = status_of(kind);
    const auto fill = [&](Event &event) { describe_failure(event, model, status, text); };
    env.events().emit(EventKind::Message, EventLevel::Error, fill, hooks);
}

inline void report(Env *env,
                   const BIYOS_model *model,
                   Hooks *hooks,
                   ErrorKind kind,
                   std::string_view text) noexcept
{
    if (env == nullptr)
    {
        return;
    }
    env->record_error(kind, text, model);
    if (!env->events().delivering())
    {
        announce(*env, model, hooks, kind, text);
    }
}

inline BIYOS_status fail(Env *env,
                         const BIYOS_model *model,
                         Hooks *hooks,
                         ErrorKind kind,
                         std::string_view message) noexcept
{
    if (env != nullptr)
    {
        try
        {
            report(env, model, hooks, kind, Error(kind, "", std::string(message)).what());
        }
        catch (...)
        {
            report(env, model, hooks, kind, kind_name(kind));
        }
    }
    return status_of(kind);
}

template <std::invocable Body>
BIYOS_status guard(Env *env, const BIYOS_model *model, Hooks *hooks, Body &&body) noexcept
{
    try
    {
        body();
        return BIYOS_OK;
    }
    catch (const Error &error)
    {
        report(env, model, hooks, error.kind(), error.what());
        return status_of(error.kind());
    }
    catch (const std::bad_alloc &)
    {
        report(env, model, hooks, ErrorKind::NoMemory, kind_name(ErrorKind::NoMemory));
        return status_of(ErrorKind::NoMemory);
    }
    catch (const std::exception &error)
    {
        return fail(env, model, hooks, ErrorKind::Internal, error.what());
    }
    catch (...)
    {
        return fail(env, model, hooks, ErrorKind::Internal, "unknown exception");
    }
}

template <std::invocable Body> BIYOS_status guard(Body &&body) noexcept
{
    return guard(nullptr, nullptr, nullptr, std::forward<Body>(body));
}

}
