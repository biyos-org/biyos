#include "biyos/biyos.h"

#include "capi/entry.hpp"
#include "capi/firewall.hpp"
#include "core/env.hpp"
#include "core/error.hpp"

#include <cstddef>
#include <string>

using biyos::capi::guarded;
using biyos::capi::writable;

namespace
{

constexpr std::size_t kErrorSize = offsetof(BIYOS_error, msg) + sizeof(BIYOS_error::msg);

void freeable(biyos::Env &env)
{
    if (writable(env).handles() > 0)
    {
        const std::string reason = "has a model that is not freed";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, "environment", reason);
    }
}

}

BIYOS_status BIYOS_newenv(BIYOS_env **env)
{
    if (env == nullptr)
    {
        return BIYOS_ERR_BADARG;
    }
    *env = nullptr;
    return biyos::capi::guard([&] { *env = new BIYOS_env; });
}

BIYOS_status BIYOS_freeenv(BIYOS_env *env)
{
    if (env == nullptr)
    {
        return BIYOS_OK;
    }
    const BIYOS_status status = guarded<freeable>(env);
    if (status == BIYOS_OK)
    {
        delete env;
    }
    return status;
}

BIYOS_status BIYOS_lasterror(const BIYOS_env *env, BIYOS_error *err)
{
    if (env == nullptr || err == nullptr || err->size < kErrorSize)
    {
        return BIYOS_ERR_BADARG;
    }
    const biyos::RecordView record = env->core.last_error();
    err->status = record.status;
    err->mdl = record.model;
    err->msg = record.text;
    return BIYOS_OK;
}

BIYOS_status BIYOS_interrupt(BIYOS_env *env)
{
    if (env == nullptr)
    {
        return BIYOS_ERR_BADARG;
    }
    env->core.interrupt().set();
    return BIYOS_OK;
}
