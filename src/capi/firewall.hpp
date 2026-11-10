#pragma once

#include "biyos/biyos.h"
#include "core/environment.hpp"
#include "core/error.hpp"

#include <concepts>
#include <exception>
#include <new>
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
        report(env, "out of memory");
        return BIYOS_NO_MEMORY;
    }
    catch (const std::exception &error)
    {
        report(env, error.what());
        return BIYOS_INTERNAL_ERROR;
    }
    catch (...)
    {
        report(env, "unknown exception");
        return BIYOS_INTERNAL_ERROR;
    }
}

template <std::invocable Body> biyos_status guard(Body &&body) noexcept
{
    return guard(nullptr, std::forward<Body>(body));
}

}
