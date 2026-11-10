#include "biyos/biyos.h"

#include "capi/firewall.hpp"
#include "core/environment.hpp"
#include "core/error.hpp"
#include "core/options.hpp"
#include "core/version.hpp"

#include <string>
#include <string_view>

struct biyos_env
{
    biyos::Environment core;
};

namespace
{

template <typename T> T *require(T *pointer, std::string_view name)
{
    if (pointer == nullptr)
    {
        throw biyos::Error(biyos::ErrorKind::InvalidArgument,
                           "argument '" + std::string(name) + "'", "is null");
    }
    return pointer;
}

void option_id(biyos::Options &, const char *name, int *option)
{
    const biyos::Option found = biyos::find_option(std::string_view(require(name, "name")));
    *require(option, "option") = static_cast<int>(found);
}

void set_bool(biyos::Options &options, int option, int value)
{
    options.set_bool(biyos::find_option(option), value != 0);
}

void set_int(biyos::Options &options, int option, int value)
{
    options.set_int(biyos::find_option(option), value);
}

void set_double(biyos::Options &options, int option, double value)
{
    options.set_double(biyos::find_option(option), value);
}

void set_string(biyos::Options &options, int option, const char *value)
{
    const biyos::Option found = biyos::find_option(option);
    options.set_string(found, require(value, "value"));
}

void get_bool(biyos::Options &options, int option, int *value)
{
    const bool found = options.get_bool(biyos::find_option(option));
    *require(value, "value") = found ? 1 : 0;
}

void get_int(biyos::Options &options, int option, int *value)
{
    const int found = options.get_int(biyos::find_option(option));
    *require(value, "value") = found;
}

void get_double(biyos::Options &options, int option, double *value)
{
    const double found = options.get_double(biyos::find_option(option));
    *require(value, "value") = found;
}

void get_string(biyos::Options &options, int option, const char **value)
{
    const char *found = options.get_string(biyos::find_option(option)).c_str();
    *require(value, "value") = found;
}

template <auto Body, typename... Args> biyos_status guarded(biyos_env *env, Args... args) noexcept
{
    if (env == nullptr)
    {
        return BIYOS_INVALID_ARGUMENT;
    }
    return biyos::capi::guard(&env->core, [&] { Body(env->core.options(), args...); });
}

}

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

biyos_status biyos_option_id(biyos_env *env, const char *name, int *option)
{
    return guarded<option_id>(env, name, option);
}

biyos_status biyos_set_bool_option(biyos_env *env, int option, int value)
{
    return guarded<set_bool>(env, option, value);
}

biyos_status biyos_set_int_option(biyos_env *env, int option, int value)
{
    return guarded<set_int>(env, option, value);
}

biyos_status biyos_set_double_option(biyos_env *env, int option, double value)
{
    return guarded<set_double>(env, option, value);
}

biyos_status biyos_set_string_option(biyos_env *env, int option, const char *value)
{
    return guarded<set_string>(env, option, value);
}

biyos_status biyos_get_bool_option(biyos_env *env, int option, int *value)
{
    return guarded<get_bool>(env, option, value);
}

biyos_status biyos_get_int_option(biyos_env *env, int option, int *value)
{
    return guarded<get_int>(env, option, value);
}

biyos_status biyos_get_double_option(biyos_env *env, int option, double *value)
{
    return guarded<get_double>(env, option, value);
}

biyos_status biyos_get_string_option(biyos_env *env, int option, const char **value)
{
    return guarded<get_string>(env, option, value);
}
