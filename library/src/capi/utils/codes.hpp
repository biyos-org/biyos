#pragma once

#include "biyos/biyos.h"
#include "core/env/error.hpp"

namespace biyos::capi
{

inline BIYOS_status status_of(ErrorKind kind) noexcept
{
    switch (kind)
    {
    case ErrorKind::InvalidArgument:
        return BIYOS_ERR_BADARG;
    case ErrorKind::BadFile:
        return BIYOS_ERR_BADFILE;
    case ErrorKind::Unsupported:
        return BIYOS_ERR_UNSUPPORTED;
    case ErrorKind::Busy:
        return BIYOS_ERR_BUSY;
    case ErrorKind::InUse:
        return BIYOS_ERR_INUSE;
    case ErrorKind::Callback:
        return BIYOS_ERR_CALLBACK;
    case ErrorKind::NoMemory:
        return BIYOS_ERR_NOMEMORY;
    case ErrorKind::FileAccess:
        return BIYOS_ERR_FILEIO;
    case ErrorKind::Internal:
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_ERR_INTERNAL;
}

}
