#pragma once

#include "biyos/biyos.h"
#include "core/env.hpp"
#include "core/error.hpp"
#include "core/events.hpp"

#include <concepts>
#include <exception>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace biyos::capi
{

inline void describe_failure(Event &event, BIYOS_status status, std::string_view text)
{
    event.module = "capi";
    event.text.assign(text);
    event.fields.push_back(Field{.name = "code", .value = std::to_string(status)});
}

inline void announce(Env &env, ErrorKind kind, std::string_view text) noexcept
{
    const BIYOS_status status = status_of(kind);
    const auto fill = [status, text](Event &event) { describe_failure(event, status, text); };
    env.events().emit(EventKind::Message, EventLevel::Error, fill);
}

inline void report(Env *env, ErrorKind kind, std::string_view text) noexcept
{
    if (env == nullptr)
    {
        return;
    }
    env->record_error(kind, text);
    if (!env->events().delivering())
    {
        announce(*env, kind, text);
    }
}

inline BIYOS_status fail(Env *env, ErrorKind kind, std::string_view message) noexcept
{
    if (env != nullptr)
    {
        try
        {
            report(env, kind, Error(kind, "", std::string(message)).what());
        }
        catch (...)
        {
            report(env, kind, kind_name(kind));
        }
    }
    return status_of(kind);
}

template <std::invocable Body> BIYOS_status guard(Env *env, Body &&body) noexcept
{
    try
    {
        body();
        return BIYOS_OK;
    }
    catch (const Error &error)
    {
        report(env, error.kind(), error.what());
        return status_of(error.kind());
    }
    catch (const std::bad_alloc &)
    {
        report(env, ErrorKind::NoMemory, kind_name(ErrorKind::NoMemory));
        return status_of(ErrorKind::NoMemory);
    }
    catch (const std::exception &error)
    {
        return fail(env, ErrorKind::Internal, error.what());
    }
    catch (...)
    {
        return fail(env, ErrorKind::Internal, "unknown exception");
    }
}

template <std::invocable Body> BIYOS_status guard(Body &&body) noexcept
{
    return guard(nullptr, std::forward<Body>(body));
}

}
