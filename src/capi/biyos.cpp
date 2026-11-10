#include "biyos/biyos.h"

#include "capi/firewall.hpp"
#include "core/environment.hpp"
#include "core/version.hpp"

struct biyos_env
{
    biyos::Environment core;
};

void biyos_version(int *major, int *minor, int *patch)
{
    const biyos::Version v = biyos::version();
    if (major != nullptr)
    {
        *major = v.major;
    }
    if (minor != nullptr)
    {
        *minor = v.minor;
    }
    if (patch != nullptr)
    {
        *patch = v.patch;
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

void biyos_env_free(biyos_env *env)
{
    delete env;
}

const char *biyos_last_error_message(const biyos_env *env)
{
    return env == nullptr ? "" : env->core.last_error().c_str();
}
