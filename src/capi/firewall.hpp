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

inline BIYOS_status status_of(ErrorKind knd) noexcept
{
    switch (knd)
    {
    case ErrorKind::InvalidArgument:
        return BIYOS_ERR_BADARG;
    case ErrorKind::NoMemory:
        return BIYOS_ERR_NOMEMORY;
    case ErrorKind::Internal:
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_ERR_INTERNAL;
}

inline void describe_failure(Event &evt, BIYOS_status status, std::string_view text)
{
    evt.module = "capi";
    evt.text.assign(text);
    evt.fields.push_back(Field{.name = "code", .value = std::to_string(status)});
}

inline void announce(Env &env, ErrorKind knd, std::string_view text) noexcept
{
    const BIYOS_status status = status_of(knd);
    const auto fill = [status, text](Event &evt) { describe_failure(evt, status, text); };
    env.events().emit(EventKind::Message, EventLevel::Error, fill);
}

inline void report(Env *env, ErrorKind knd, std::string_view text) noexcept
{
    if (env == nullptr)
    {
        return;
    }
    env->record_error(text);
    if (!env->events().delivering())
    {
        announce(*env, knd, text);
    }
}

inline BIYOS_status fail(Env *env, ErrorKind knd, std::string_view msg) noexcept
{
    if (env != nullptr)
    {
        try
        {
            report(env, knd, Error(knd, "", std::string(msg)).what());
        }
        catch (...)
        {
            report(env, knd, kind_name(knd));
        }
    }
    return status_of(knd);
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
