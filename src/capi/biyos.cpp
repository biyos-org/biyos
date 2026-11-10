#include "biyos/biyos.h"

#include "capi/entry.hpp"
#include "capi/event.hpp"
#include "capi/firewall.hpp"
#include "core/environment.hpp"
#include "core/events.hpp"

#include <cstddef>
#include <vector>

using biyos::capi::guarded;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

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
