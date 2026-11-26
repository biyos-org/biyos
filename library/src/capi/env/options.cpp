#include "biyos/biyos.h"

#include "capi/utils/entry.hpp"
#include "core/env.hpp"
#include "core/events/stream.hpp"
#include "core/options/lookup.hpp"
#include "core/options/options.hpp"
#include "core/options/settings.hpp"
#include "core/version.hpp"

#include <optional>
#include <string_view>

using biyos::Env;
using biyos::find_option;
using biyos::kind_of;
using biyos::Masks;
using biyos::number_of;
using biyos::Option;
using biyos::OptionKind;
using biyos::Options;
using biyos::OptionSource;
using biyos::Session;
using biyos::Settings;
using biyos::Version;
using biyos::Versions;
using biyos::versions_of;
using biyos::capi::guarded;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

void option_id(Env &, const char *name, BIYOS_int *option)
{
    const Option found = find_option(std::string_view(require(name, "name")));
    *require(option, "opt") = static_cast<BIYOS_int>(found);
}

void option_kind(Env &, BIYOS_int option, BIYOS_int *kind)
{
    const OptionKind found = kind_of(find_option(option));
    *require(kind, "knd") = static_cast<BIYOS_int>(found);
}

BIYOS_int optional_number(std::optional<Version> version)
{
    return version ? number_of(*version) : 0;
}

void option_versions(Env &,
                     BIYOS_int option,
                     BIYOS_int *since,
                     BIYOS_int *deprecated,
                     BIYOS_int *retired)
{
    const Versions versions = versions_of(find_option(option));
    require(since, "since");
    require(deprecated, "deprecated");
    require(retired, "retired");
    *since = number_of(versions.since);
    *deprecated = optional_number(versions.deprecated);
    *retired = optional_number(versions.retired);
}

void set_bool(Env &env, BIYOS_int option, BIYOS_int value)
{
    writable(env).options().set_bool(find_option(option), value != 0);
}

void set_int(Env &env, BIYOS_int option, BIYOS_int value)
{
    writable(env).options().set_int(find_option(option), value);
}

void set_double(Env &env, BIYOS_int option, BIYOS_real value)
{
    writable(env).options().set_double(find_option(option), value);
}

void set_string(Env &env, BIYOS_int option, const char *value)
{
    Options &options = writable(env).options();
    const Option found = find_option(option);
    options.set_string(found, require(value, "val"));
}

void set_flags(Env &env, BIYOS_int option, BIYOS_mask on, BIYOS_mask off)
{
    Options &options = writable(env).options();
    options.set_flags(find_option(option), Masks{.on = on, .off = off});
}

void get_bool(Env &env, BIYOS_int option, BIYOS_int *value)
{
    const bool found = env.options().get_bool(find_option(option));
    *require(value, "val") = found ? 1 : 0;
}

void get_int(Env &env, BIYOS_int option, BIYOS_int *value)
{
    const BIYOS_int found = env.options().get_int(find_option(option));
    *require(value, "val") = found;
}

void get_double(Env &env, BIYOS_int option, BIYOS_real *value)
{
    const BIYOS_real found = env.options().get_double(find_option(option));
    *require(value, "val") = found;
}

void get_string(Env &env, BIYOS_int option, const char **value)
{
    const char *found = env.options().get_string(find_option(option)).data();
    *require(value, "val") = found;
}

void get_flags(Env &env, BIYOS_int option, BIYOS_mask *on, BIYOS_mask *off)
{
    const Masks found = env.options().get_flags(find_option(option));
    require(on, "on");
    require(off, "off");
    *on = found.on;
    *off = found.off;
}

void delete_option(Env &env, BIYOS_int option)
{
    writable(env).options().drop(find_option(option));
}

Settings &settings_of(Session &session)
{
    writable(session);
    return session.settings();
}

void set_model_bool(Session &session, BIYOS_int option, BIYOS_int value)
{
    Settings &settings = settings_of(session);
    const Option found = find_option(option);
    settings.set_bool(found, value != 0, session.stream());
}

void set_model_int(Session &session, BIYOS_int option, BIYOS_int value)
{
    Settings &settings = settings_of(session);
    const Option found = find_option(option);
    settings.set_int(found, value, session.stream());
}

void set_model_double(Session &session, BIYOS_int option, BIYOS_real value)
{
    Settings &settings = settings_of(session);
    const Option found = find_option(option);
    settings.set_double(found, value, session.stream());
}

void set_model_string(Session &session, BIYOS_int option, const char *value)
{
    Settings &settings = settings_of(session);
    const Option found = find_option(option);
    settings.set_string(found, require(value, "val"), session.stream());
}

void set_model_flags(Session &session, BIYOS_int option, BIYOS_mask on, BIYOS_mask off)
{
    Settings &settings = settings_of(session);
    const Masks value{.on = on, .off = off};
    settings.set_flags(find_option(option), value, session.stream());
}

void get_model_bool(const Session &session, BIYOS_int option, BIYOS_int *value)
{
    const bool found = session.lookup().get_bool(find_option(option));
    *require(value, "val") = found ? 1 : 0;
}

void get_model_int(const Session &session, BIYOS_int option, BIYOS_int *value)
{
    const BIYOS_int found = session.lookup().get_int(find_option(option));
    *require(value, "val") = found;
}

void get_model_double(const Session &session, BIYOS_int option, BIYOS_real *value)
{
    const BIYOS_real found = session.lookup().get_double(find_option(option));
    *require(value, "val") = found;
}

void get_model_string(const Session &session, BIYOS_int option, const char **value)
{
    const char *found = session.lookup().get_string(find_option(option)).data();
    *require(value, "val") = found;
}

void get_model_flags(const Session &session, BIYOS_int option, BIYOS_mask *on, BIYOS_mask *off)
{
    const Masks found = session.lookup().get_flags(find_option(option));
    require(on, "on");
    require(off, "off");
    *on = found.on;
    *off = found.off;
}

void delete_model_option(Session &session, BIYOS_int option)
{
    settings_of(session).erase(find_option(option));
}

void get_model_source(const Session &session, BIYOS_int option, BIYOS_int *source)
{
    const OptionSource found = session.lookup().find(find_option(option)).source;
    *require(source, "src") = static_cast<BIYOS_int>(found);
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

BIYOS_status BIYOS_setenvflgopt(BIYOS_env *env, BIYOS_int opt, BIYOS_mask on, BIYOS_mask off)
{
    return guarded<set_flags>(env, opt, on, off);
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

BIYOS_status BIYOS_getenvflgopt(BIYOS_env *env, BIYOS_int opt, BIYOS_mask *on, BIYOS_mask *off)
{
    return guarded<get_flags>(env, opt, on, off);
}

BIYOS_status BIYOS_delenvopt(BIYOS_env *env, BIYOS_int opt)
{
    return guarded<delete_option>(env, opt);
}

BIYOS_status BIYOS_setmdlboolopt(BIYOS_model *mdl, BIYOS_int opt, BIYOS_int val)
{
    return guarded<set_model_bool>(mdl, opt, val);
}

BIYOS_status BIYOS_setmdlintopt(BIYOS_model *mdl, BIYOS_int opt, BIYOS_int val)
{
    return guarded<set_model_int>(mdl, opt, val);
}

BIYOS_status BIYOS_setmdldblopt(BIYOS_model *mdl, BIYOS_int opt, BIYOS_real val)
{
    return guarded<set_model_double>(mdl, opt, val);
}

BIYOS_status BIYOS_setmdlstropt(BIYOS_model *mdl, BIYOS_int opt, const char *val)
{
    return guarded<set_model_string>(mdl, opt, val);
}

BIYOS_status BIYOS_setmdlflgopt(BIYOS_model *mdl, BIYOS_int opt, BIYOS_mask on, BIYOS_mask off)
{
    return guarded<set_model_flags>(mdl, opt, on, off);
}

BIYOS_status BIYOS_getmdlboolopt(const BIYOS_model *mdl, BIYOS_int opt, BIYOS_int *val)
{
    return guarded<get_model_bool>(mdl, opt, val);
}

BIYOS_status BIYOS_getmdlintopt(const BIYOS_model *mdl, BIYOS_int opt, BIYOS_int *val)
{
    return guarded<get_model_int>(mdl, opt, val);
}

BIYOS_status BIYOS_getmdldblopt(const BIYOS_model *mdl, BIYOS_int opt, BIYOS_real *val)
{
    return guarded<get_model_double>(mdl, opt, val);
}

BIYOS_status BIYOS_getmdlstropt(const BIYOS_model *mdl, BIYOS_int opt, const char **val)
{
    return guarded<get_model_string>(mdl, opt, val);
}

BIYOS_status BIYOS_getmdlflgopt(const BIYOS_model *mdl,
                                BIYOS_int opt,
                                BIYOS_mask *on,
                                BIYOS_mask *off)
{
    return guarded<get_model_flags>(mdl, opt, on, off);
}

BIYOS_status BIYOS_delmdlopt(BIYOS_model *mdl, BIYOS_int opt)
{
    return guarded<delete_model_option>(mdl, opt);
}

BIYOS_status BIYOS_getmdloptsrc(const BIYOS_model *mdl, BIYOS_int opt, BIYOS_int *src)
{
    return guarded<get_model_source>(mdl, opt, src);
}
