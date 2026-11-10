#include "biyos/biyos.h"

#include "capi/entry.hpp"
#include "capi/firewall.hpp"
#include "core/environment.hpp"
#include "core/error.hpp"

#include <string>

using biyos::capi::guarded;
using biyos::capi::writable;

namespace
{

void freeable(biyos::Environment &env)
{
    if (writable(env).handles() > 0)
    {
        const std::string reason = "has a model that is not freed";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, "environment", reason);
    }
}

}

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
    const biyos_status status = guarded<freeable>(env);
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
