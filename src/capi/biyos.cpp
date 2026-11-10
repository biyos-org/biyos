#include "biyos/biyos.h"

#include "capi/event.hpp"
#include "capi/firewall.hpp"
#include "core/environment.hpp"
#include "core/error.hpp"
#include "core/events.hpp"
#include "core/options.hpp"
#include "core/version.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

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

biyos::Environment &writable(biyos::Environment &env)
{
    if (env.events().delivering())
    {
        const std::string reason = "is read-only while delivering an event";
        throw biyos::Error(biyos::ErrorKind::InvalidArgument, "environment", reason);
    }
    return env;
}

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
    biyos_subscriber subscriber;
    void *user_data;

    void operator()(const biyos::Event &event) const
    {
        const biyos_event view{&event};
        subscriber(&view, user_data);
    }
};

void subscribe(biyos::Environment &env, biyos_subscriber subscriber, void *user_data, int kind,
               int lowest, int *subscription)
{
    biyos::Events &events = writable(env).events();
    const Forwarder forwarder{require(subscriber, "subscriber"), user_data};
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

template <auto Body, typename... Args> biyos_status guarded(biyos_env *env, Args... args) noexcept
{
    if (env == nullptr)
    {
        return BIYOS_INVALID_ARGUMENT;
    }
    return biyos::capi::guard(&env->core, [&] { Body(env->core, args...); });
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

biyos_status biyos_option_kind(biyos_env *env, int option, int *kind)
{
    return guarded<option_kind>(env, option, kind);
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
