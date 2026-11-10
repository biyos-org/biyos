#include "biyos/biyos.h"

#include "capi/entry.hpp"
#include "capi/event.hpp"
#include "capi/firewall.hpp"
#include "core/environment.hpp"
#include "core/events.hpp"
#include "core/options.hpp"
#include "core/version.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

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

struct Forwarder
{
    biyos::Environment *env;
    biyos_subscriber subscriber;
    void *user_data;

    void operator()(const biyos::Event &event) const
    {
        const biyos::Environment::ErrorScope scope(*env);
        const biyos_event view{&event};
        subscriber(&view, user_data);
    }
};

void subscribe(biyos::Environment &env, biyos_subscriber subscriber, void *user_data, int kind,
               int lowest, int *subscription)
{
    biyos::Events &events = writable(env).events();
    const Forwarder forwarder{&env, require(subscriber, "subscriber"), user_data};
    const biyos::EventKind found = biyos::find_kind(kind);
    const biyos::Level level = biyos::find_level(lowest);
    int *id = require(subscription, "subscription");
    *id = events.subscribe(found, level, forwarder);
}

void unsubscribe(biyos::Environment &env, int subscription)
{
    writable(env).events().unsubscribe(subscription);
}

const biyos::Field *field_of(const biyos_event *event, int field)
{
    if (event == nullptr || field < 0)
    {
        return nullptr;
    }
    const std::vector<biyos::Field> &fields = event->core->fields;
    const auto index = static_cast<std::size_t>(field);
    return index < fields.size() ? &fields[index] : nullptr;
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

biyos_status biyos_option_id(biyos_env *env, const char *name, int *option)
{
    return guarded<option_id>(env, name, option);
}

biyos_status biyos_option_kind(biyos_env *env, int option, int *kind)
{
    return guarded<option_kind>(env, option, kind);
}

biyos_status biyos_option_versions(biyos_env *env, int option, int *since, int *deprecated,
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

biyos_status biyos_subscribe(biyos_env *env, biyos_subscriber subscriber, void *user_data, int kind,
                             int lowest, int *subscription)
{
    return guarded<subscribe>(env, subscriber, user_data, kind, lowest, subscription);
}

biyos_status biyos_unsubscribe(biyos_env *env, int subscription)
{
    return guarded<unsubscribe>(env, subscription);
}

int biyos_event_kind(const biyos_event *event)
{
    return event == nullptr ? 0 : static_cast<int>(event->core->kind);
}

int biyos_event_level(const biyos_event *event)
{
    return event == nullptr ? 0 : static_cast<int>(event->core->level);
}

const char *biyos_event_module(const biyos_event *event)
{
    return event == nullptr ? "" : event->core->module.c_str();
}

const char *biyos_event_text(const biyos_event *event)
{
    return event == nullptr ? "" : event->core->text.c_str();
}

int biyos_event_field_count(const biyos_event *event)
{
    return event == nullptr ? 0 : static_cast<int>(event->core->fields.size());
}

const char *biyos_event_field_name(const biyos_event *event, int field)
{
    const biyos::Field *found = field_of(event, field);
    return found == nullptr ? "" : found->name.c_str();
}

const char *biyos_event_field_value(const biyos_event *event, int field)
{
    const biyos::Field *found = field_of(event, field);
    return found == nullptr ? "" : found->value.c_str();
}
