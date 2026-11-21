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

void opt_id(biyos::Env &, const char *name, int *opt)
{
    const biyos::Option found = biyos::find_opt(std::string_view(require(name, "name")));
    *require(opt, "opt") = static_cast<int>(found);
}

void option_kind(biyos::Env &, int opt, int *knd)
{
    const biyos::OptionKind found = biyos::kind_of(biyos::find_opt(opt));
    *require(knd, "knd") = static_cast<int>(found);
}

int optional_number(std::optional<biyos::Version> version)
{
    return version ? biyos::number_of(*version) : 0;
}

void opt_versions(biyos::Env &, int opt, int *since, int *deprecated, int *retired)
{
    const biyos::Versions versions = biyos::versions_of(biyos::find_opt(opt));
    require(since, "since");
    require(deprecated, "deprecated");
    require(retired, "retired");
    *since = biyos::number_of(versions.since);
    *deprecated = optional_number(versions.deprecated);
    *retired = optional_number(versions.retired);
}

void set_bool(biyos::Env &env, int opt, int value)
{
    writable(env).options().set_bool(biyos::find_opt(opt), value != 0);
}

void set_int(biyos::Env &env, int opt, int value)
{
    writable(env).options().set_int(biyos::find_opt(opt), value);
}

void set_dbl(biyos::Env &env, int opt, double value)
{
    writable(env).options().set_dbl(biyos::find_opt(opt), value);
}

void set_str(biyos::Env &env, int opt, const char *value)
{
    biyos::Options &opts = writable(env).options();
    const biyos::Option found = biyos::find_opt(opt);
    opts.set_str(found, require(value, "val"));
}

void get_bool(biyos::Env &env, int opt, int *value)
{
    const bool found = env.options().get_bool(biyos::find_opt(opt));
    *require(value, "val") = found ? 1 : 0;
}

void get_int(biyos::Env &env, int opt, int *value)
{
    const int found = env.options().get_int(biyos::find_opt(opt));
    *require(value, "val") = found;
}

void get_dbl(biyos::Env &env, int opt, double *value)
{
    const double found = env.options().get_dbl(biyos::find_opt(opt));
    *require(value, "val") = found;
}

void get_str(biyos::Env &env, int opt, const char **value)
{
    const char *found = env.options().get_str(biyos::find_opt(opt)).c_str();
    *require(value, "val") = found;
}

}

BIYOS_status BIYOS_getoptid(BIYOS_env *env, const char *name, BIYOS_int *opt)
{
    return guarded<opt_id>(env, name, opt);
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
    return guarded<opt_versions>(env, opt, since, deprecated, retired);
}

BIYOS_status BIYOS_setboolopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int val)
{
    return guarded<set_bool>(env, opt, val);
}

BIYOS_status BIYOS_setintopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int val)
{
    return guarded<set_int>(env, opt, val);
}

BIYOS_status BIYOS_setdblopt(BIYOS_env *env, BIYOS_int opt, BIYOS_real val)
{
    return guarded<set_dbl>(env, opt, val);
}

BIYOS_status BIYOS_setstropt(BIYOS_env *env, BIYOS_int opt, const char *val)
{
    return guarded<set_str>(env, opt, val);
}

BIYOS_status BIYOS_getboolopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int *val)
{
    return guarded<get_bool>(env, opt, val);
}

BIYOS_status BIYOS_getintopt(BIYOS_env *env, BIYOS_int opt, BIYOS_int *val)
{
    return guarded<get_int>(env, opt, val);
}

BIYOS_status BIYOS_getdblopt(BIYOS_env *env, BIYOS_int opt, BIYOS_real *val)
{
    return guarded<get_dbl>(env, opt, val);
}

BIYOS_status BIYOS_getstropt(BIYOS_env *env, BIYOS_int opt, const char **val)
{
    return guarded<get_str>(env, opt, val);
}
