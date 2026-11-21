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
        const BIYOS_event view{&event};
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
    const Forwarder forwarder{&env, require(subscriber, "hook"), data};
    const biyos::EventKind found = biyos::find_kind(kind);
    const biyos::Level level = biyos::find_level(lowest);
    int *id = require(subscription, "hookid");
    *id = events.subscribe(found, level, forwarder);
}

void unsubscribe(biyos::Environment &env, int subscription)
{
    writable(env).events().unsubscribe(subscription);
}

const biyos::Field *field_of(const BIYOS_event *event, int field)
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
                           BIYOS_hook hook,
                           void *data,
                           BIYOS_int kind,
                           BIYOS_int lowest,
                           BIYOS_index *hookid)
{
    return guarded<subscribe>(env, hook, data, kind, lowest, hookid);
}

BIYOS_status BIYOS_delhook(BIYOS_env *env, BIYOS_index hookid)
{
    return guarded<unsubscribe>(env, hookid);
}

BIYOS_int BIYOS_getevtknd(const BIYOS_event *evt)
{
    return evt == nullptr ? 0 : static_cast<int>(evt->core->kind);
}

BIYOS_int BIYOS_getevtlvl(const BIYOS_event *evt)
{
    return evt == nullptr ? 0 : static_cast<int>(evt->core->level);
}

const char *BIYOS_getevtmodule(const BIYOS_event *evt)
{
    return evt == nullptr ? "" : evt->core->module.c_str();
}

const char *BIYOS_getevttext(const BIYOS_event *evt)
{
    return evt == nullptr ? "" : evt->core->text.c_str();
}

BIYOS_index BIYOS_getnevtfields(const BIYOS_event *evt)
{
    return evt == nullptr ? 0 : static_cast<int>(evt->core->fields.size());
}

const char *BIYOS_getevtfieldname(const BIYOS_event *evt, BIYOS_index field)
{
    const biyos::Field *found = field_of(evt, field);
    return found == nullptr ? "" : found->name.c_str();
}

const char *BIYOS_getevtfieldvalue(const BIYOS_event *evt, BIYOS_index field)
{
    const biyos::Field *found = field_of(evt, field);
    return found == nullptr ? "" : found->value.c_str();
}
