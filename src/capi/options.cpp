#include "biyos/biyos.h"

#include "capi/entry.hpp"
#include "core/env.hpp"
#include "core/options.hpp"
#include "core/version.hpp"

#include <optional>
#include <string_view>

using biyos::capi::guarded;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

void option_id(biyos::Env &, const char *name, int *option)
{
    const biyos::Option found = biyos::find_option(std::string_view(require(name, "name")));
    *require(option, "opt") = static_cast<int>(found);
}

void option_kind(biyos::Env &, int option, int *kind)
{
    const biyos::OptionKind found = biyos::kind_of(biyos::find_option(option));
    *require(kind, "knd") = static_cast<int>(found);
}

int optional_number(std::optional<biyos::Version> version)
{
    return version ? biyos::number_of(*version) : 0;
}

void option_versions(biyos::Env &, int option, int *since, int *deprecated, int *retired)
{
    const biyos::Versions versions = biyos::versions_of(biyos::find_option(option));
    require(since, "since");
    require(deprecated, "deprecated");
    require(retired, "retired");
    *since = biyos::number_of(versions.since);
    *deprecated = optional_number(versions.deprecated);
    *retired = optional_number(versions.retired);
}

void set_bool(biyos::Env &env, int option, int value)
{
    writable(env).options().set_bool(biyos::find_option(option), value != 0);
}

void set_int(biyos::Env &env, int option, int value)
{
    writable(env).options().set_int(biyos::find_option(option), value);
}

void set_double(biyos::Env &env, int option, double value)
{
    writable(env).options().set_double(biyos::find_option(option), value);
}

void set_string(biyos::Env &env, int option, const char *value)
{
    biyos::Options &options = writable(env).options();
    const biyos::Option found = biyos::find_option(option);
    options.set_string(found, require(value, "val"));
}

void get_bool(biyos::Env &env, int option, int *value)
{
    const bool found = env.options().get_bool(biyos::find_option(option));
    *require(value, "val") = found ? 1 : 0;
}

void get_int(biyos::Env &env, int option, int *value)
{
    const int found = env.options().get_int(biyos::find_option(option));
    *require(value, "val") = found;
}

void get_double(biyos::Env &env, int option, double *value)
{
    const double found = env.options().get_double(biyos::find_option(option));
    *require(value, "val") = found;
}

void get_string(biyos::Env &env, int option, const char **value)
{
    const char *found = env.options().get_string(biyos::find_option(option)).c_str();
    *require(value, "val") = found;
}

}

BIYOS_status BIYOS_getoptid(BIYOS_env *env, const char *name, BIYOS_int *opt)
{
    return guarded<option_id>(env, name, opt);
}

BIYOS_status BIYOS_getoptknd(BIYOS_env *env, BIYOS_int opt, BIYOS_int *knd)
{
    return guarded<option_kind>(env, opt, knd);
}

BIYOS_status BIYOS_getoptver(BIYOS_env *env,
                             BIYOS_int opt,
                             BIYOS_int *since,
                             BIYOS_int *deprecated,
                             BIYOS_int *retired)
{
    return guarded<option_versions>(env, opt, since, deprecated, retired);
}

BIYOS_status BIYOS_setenvboolopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int val)
{
    return guarded<set_bool>(env, opt, val);
}

BIYOS_status BIYOS_setenvintopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int val)
{
    return guarded<set_int>(env, opt, val);
}

BIYOS_status BIYOS_setenvdblopt(BIYOS_env *env, BIYOS_int opt, BIYOS_real val)
{
    return guarded<set_double>(env, opt, val);
}

BIYOS_status BIYOS_setenvstropt(BIYOS_env *env, BIYOS_int opt, const char *val)
{
    return guarded<set_string>(env, opt, val);
}

BIYOS_status BIYOS_getenvboolopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int *val)
{
    return guarded<get_bool>(env, opt, val);
}

BIYOS_status BIYOS_getenvintopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int *val)
{
    return guarded<get_int>(env, opt, val);
}

BIYOS_status BIYOS_getenvdblopt(BIYOS_env *env, BIYOS_int opt, BIYOS_real *val)
{
    return guarded<get_double>(env, opt, val);
}

BIYOS_status BIYOS_getenvstropt(BIYOS_env *env, BIYOS_int opt, const char **val)
{
    return guarded<get_string>(env, opt, val);
}
