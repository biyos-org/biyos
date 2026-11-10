#include "biyos/biyos.h"

#include "capi/entry.hpp"
#include "capi/firewall.hpp"
#include "core/environment.hpp"

using biyos::capi::guarded;
using biyos::capi::writable;

biyos_status biyos_env_create(biyos_env **env)
{
    if (env == nullptr)
    {
        return BIYOS_INVALID_ARGUMENT;
    }
    *env = nullptr;
    return biyos::capi::guard([&] { *env = new biyos_env; });
}

biyos_status biyos_env_free(biyos_env *env)
{
    if (env == nullptr)
    {
        return BIYOS_OK;
    }
    const biyos_status status = guarded<writable>(env);
    if (status == BIYOS_OK)
    {
        delete env;
    }
    return status;
}

const char *biyos_last_error_message(const biyos_env *env)
{
    return env == nullptr ? "" : env->core.last_error().c_str();
}
