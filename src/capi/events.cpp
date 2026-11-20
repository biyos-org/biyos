#include "biyos/biyos.h"

#include "capi/entry.hpp"
#include "capi/event.hpp"
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
    BIYOS_hook subscriber;
    void *data;

    void operator()(const biyos::Event &event) const
    {
        const biyos::Environment::ErrorScope scope(*env);
        const BIYOS_evt view{&event};
        subscriber(&view, data);
    }
};

void subscribe(biyos::Environment &env,
               BIYOS_hook subscriber,
               void *data,
               int kind,
               int lowest,
               int *subscription)
{
    biyos::Events &events = writable(env).events();
    const Forwarder forwarder{&env, require(subscriber, "subscriber"), data};
    const biyos::EventKind found = biyos::find_kind(kind);
    const biyos::Level level = biyos::find_level(lowest);
    int *id = require(subscription, "subscription");
    *id = events.subscribe(found, level, forwarder);
}

void unsubscribe(biyos::Environment &env, int subscription)
{
    writable(env).events().unsubscribe(subscription);
}

const biyos::Field *field_of(const BIYOS_evt *event, int field)
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

BIYOS_status BIYOS_addhook(BIYOS_env *env,
                           BIYOS_hook subscriber,
                           void *data,
                           int kind,
                           int lowest,
                           int *subscription)
{
    return guarded<subscribe>(env, subscriber, data, kind, lowest, subscription);
}

BIYOS_status BIYOS_delhook(BIYOS_env *env, int subscription)
{
    return guarded<unsubscribe>(env, subscription);
}

int BIYOS_getevtkind(const BIYOS_evt *event)
{
    return event == nullptr ? 0 : static_cast<int>(event->core->kind);
}

int BIYOS_getevtlvl(const BIYOS_evt *event)
{
    return event == nullptr ? 0 : static_cast<int>(event->core->level);
}

const char *BIYOS_getevtmodule(const BIYOS_evt *event)
{
    return event == nullptr ? "" : event->core->module.c_str();
}

const char *BIYOS_getevttext(const BIYOS_evt *event)
{
    return event == nullptr ? "" : event->core->text.c_str();
}

int BIYOS_getnevtfields(const BIYOS_evt *event)
{
    return event == nullptr ? 0 : static_cast<int>(event->core->fields.size());
}

const char *BIYOS_getevtfieldname(const BIYOS_evt *event, int field)
{
    const biyos::Field *found = field_of(event, field);
    return found == nullptr ? "" : found->name.c_str();
}

const char *BIYOS_getevtfieldvalue(const BIYOS_evt *event, int field)
{
    const biyos::Field *found = field_of(event, field);
    return found == nullptr ? "" : found->value.c_str();
}
