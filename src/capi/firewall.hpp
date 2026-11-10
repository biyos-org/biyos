#pragma once

#include "biyos/biyos.h"
#include "core/error.hpp"

#include <concepts>
#include <new>

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

template <std::invocable Body> biyos_status guard(Body &&body) noexcept
{
    try
    {
        body();
        return BIYOS_OK;
    }
    catch (const Error &error)
    {
        return status_of(error.kind());
    }
    catch (const std::bad_alloc &)
    {
        return BIYOS_NO_MEMORY;
    }
    catch (...)
    {
        return BIYOS_INTERNAL_ERROR;
    }
}

}
