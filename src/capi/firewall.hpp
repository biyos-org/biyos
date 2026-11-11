#pragma once

#include "biyos/biyos.h"
#include "core/environment.hpp"
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

inline biyos_status status_of(ErrorKind kind) noexcept
{
    switch (kind)
    {
    case ErrorKind::InvalidArgument:
        return BIYOS_INVALID_ARGUMENT;
    case ErrorKind::NoMemory:
        return BIYOS_NO_MEMORY;
    case ErrorKind::Internal:
        return BIYOS_INTERNAL_ERROR;
    }
    return BIYOS_INTERNAL_ERROR;
}

inline void describe_failure(Event &event, biyos_status status, std::string_view text)
{
    event.module = "capi";
    event.text.assign(text);
    event.fields.push_back(Field{.name = "code", .value = std::to_string(status)});
}

inline void announce(Environment &env, ErrorKind kind, std::string_view text) noexcept
{
    const biyos_status status = status_of(kind);
    const auto fill = [status, text](Event &event) { describe_failure(event, status, text); };
    env.events().emit(EventKind::Message, Level::Error, fill);
}

inline void report(Environment *env, ErrorKind kind, std::string_view text) noexcept
{
    if (env == nullptr)
    {
        return;
    }
    env->record_error(text);
    if (!env->events().delivering())
    {
        announce(*env, kind, text);
    }
}

inline biyos_status fail(Environment *env, ErrorKind kind, std::string_view message) noexcept
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

template <std::invocable Body> biyos_status guard(Environment *env, Body &&body) noexcept
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

template <std::invocable Body> biyos_status guard(Body &&body) noexcept
{
    return guard(nullptr, std::forward<Body>(body));
}

}
