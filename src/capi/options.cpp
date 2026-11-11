#include "biyos/biyos.h"

#include "capi/entry.hpp"
#include "core/environment.hpp"
#include "core/options.hpp"
#include "core/version.hpp"

#include <optional>
#include <string_view>

using biyos::capi::guarded;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

void option_id(biyos::Environment &, const char *name, int *option)
{
    const biyos::Option found = biyos::find_option(std::string_view(require(name, "name")));
    *require(option, "option") = static_cast<int>(found);
}

void option_kind(biyos::Environment &, int option, int *kind)
{
    const biyos::OptionKind found = biyos::kind_of(biyos::find_option(option));
    *require(kind, "kind") = static_cast<int>(found);
}

int optional_number(std::optional<biyos::Version> version)
{
    return version ? biyos::number_of(*version) : 0;
}

void option_versions(biyos::Environment &, int option, int *since, int *deprecated, int *retired)
{
    const biyos::Versions versions = biyos::versions_of(biyos::find_option(option));
    require(since, "since");
    require(deprecated, "deprecated");
    require(retired, "retired");
    *since = biyos::number_of(versions.since);
    *deprecated = optional_number(versions.deprecated);
    *retired = optional_number(versions.retired);
}

void set_bool(biyos::Environment &env, int option, int value)
{
    writable(env).options().set_bool(biyos::find_option(option), value != 0);
}

void set_int(biyos::Environment &env, int option, int value)
{
    writable(env).options().set_int(biyos::find_option(option), value);
}

void set_double(biyos::Environment &env, int option, double value)
{
    writable(env).options().set_double(biyos::find_option(option), value);
}

void set_string(biyos::Environment &env, int option, const char *value)
{
    biyos::Options &options = writable(env).options();
    const biyos::Option found = biyos::find_option(option);
    options.set_string(found, require(value, "value"));
}

void get_bool(biyos::Environment &env, int option, int *value)
{
    const bool found = env.options().get_bool(biyos::find_option(option));
    *require(value, "value") = found ? 1 : 0;
}

void get_int(biyos::Environment &env, int option, int *value)
{
    const int found = env.options().get_int(biyos::find_option(option));
    *require(value, "value") = found;
}

void get_double(biyos::Environment &env, int option, double *value)
{
    const double found = env.options().get_double(biyos::find_option(option));
    *require(value, "value") = found;
}

void get_string(biyos::Environment &env, int option, const char **value)
{
    const char *found = env.options().get_string(biyos::find_option(option)).c_str();
    *require(value, "value") = found;
}

}

biyos_status biyos_option_id(biyos_env *env, const char *name, int *option)
{
    return guarded<option_id>(env, name, option);
}

biyos_status biyos_option_kind(biyos_env *env, int option, int *kind)
{
    return guarded<option_kind>(env, option, kind);
}

biyos_status biyos_option_versions(biyos_env *env,
                                   int option,
                                   int *since,
                                   int *deprecated,
                                   int *retired)
{
    return guarded<option_versions>(env, option, since, deprecated, retired);
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
