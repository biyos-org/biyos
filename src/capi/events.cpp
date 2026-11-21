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
    const Forwarder forwarder{&env, require(subscriber, "hk"), data};
    const biyos::EventKind found = biyos::find_kind(kind);
    const biyos::Level level = biyos::find_level(lowest);
    int *id = require(subscription, "idx");
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
                           BIYOS_hook hk,
                           void *data,
                           BIYOS_int knd,
                           BIYOS_int lvl,
                           BIYOS_index *idx)
{
    return guarded<subscribe>(env, hk, data, knd, lvl, idx);
}

BIYOS_status BIYOS_delhook(BIYOS_env *env, BIYOS_index idx)
{
    return guarded<unsubscribe>(env, idx);
}

BIYOS_int BIYOS_getevtknd(const BIYOS_event *evt)
{
    return evt == nullptr ? 0 : static_cast<int>(evt->core->kind);
}

BIYOS_int BIYOS_getevtlvl(const BIYOS_event *evt)
{
    return evt == nullptr ? 0 : static_cast<int>(evt->core->level);
}

const char *BIYOS_getevtmod(const BIYOS_event *evt)
{
    return evt == nullptr ? "" : evt->core->module.c_str();
}

const char *BIYOS_getevttext(const BIYOS_event *evt)
{
    return evt == nullptr ? "" : evt->core->text.c_str();
}

BIYOS_index BIYOS_getnevtflds(const BIYOS_event *evt)
{
    return evt == nullptr ? 0 : static_cast<int>(evt->core->fields.size());
}

const char *BIYOS_getevtfldname(const BIYOS_event *evt, BIYOS_index idx)
{
    const biyos::Field *found = field_of(evt, idx);
    return found == nullptr ? "" : found->name.c_str();
}

const char *BIYOS_getevtfldval(const BIYOS_event *evt, BIYOS_index idx)
{
    const biyos::Field *found = field_of(evt, idx);
    return found == nullptr ? "" : found->value.c_str();
}
