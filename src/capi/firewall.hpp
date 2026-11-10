#pragma once

#include "biyos/biyos.h"
#include "core/environment.hpp"
#include "core/error.hpp"

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

inline void report(Environment *env, std::string_view text) noexcept
{
    if (env != nullptr)
    {
        env->record_error(text);
    }
}

inline biyos_status fail(Environment *env, ErrorKind kind, std::string_view message) noexcept
{
    if (env != nullptr)
    {
        try
        {
            env->record_error(Error(kind, "", std::string(message)).what());
        }
        catch (...)
        {
            env->record_error(kind_name(kind));
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
        report(env, error.what());
        return status_of(error.kind());
    }
    catch (const std::bad_alloc &)
    {
        report(env, kind_name(ErrorKind::NoMemory));
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
