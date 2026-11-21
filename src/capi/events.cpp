#include "biyos/biyos.h"

#include "capi/entry.hpp"
#include "capi/event.hpp"
#include "core/env.hpp"
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
    biyos::Env *env;
    BIYOS_hook hook;
    void *data;

    void operator()(const biyos::Event &event) const
    {
        const biyos::Env::ErrorScope scope(*env);
        const BIYOS_event view{&event};
        hook(&view, data);
    }
};

void add_hook(biyos::Env &env, BIYOS_hook hk, void *data, int kind, int lvl, int *idx)
{
    biyos::Events &events = writable(env).events();
    const Forwarder forwarder{&env, require(hk, "hk"), data};
    const biyos::EventKind found = biyos::find_kind(kind);
    const biyos::EventLevel level = biyos::find_level(lvl);
    int *id = require(idx, "idx");
    *id = events.add_hook(found, level, forwarder);
}

void del_hook(biyos::Env &env, int idx)
{
    writable(env).events().del_hook(idx);
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
    return guarded<add_hook>(env, hk, data, knd, lvl, idx);
}

BIYOS_status BIYOS_delhook(BIYOS_env *env, BIYOS_index idx)
{
    return guarded<del_hook>(env, idx);
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
