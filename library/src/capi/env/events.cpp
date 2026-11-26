#include "biyos/biyos.h"

#include "capi/env/event.hpp"
#include "capi/utils/entry.hpp"
#include "core/env.hpp"
#include "core/events/events.hpp"
#include "core/numbers.hpp"

#include <cstddef>
#include <vector>

using biyos::Env;
using biyos::ErrorScope;
using biyos::Event;
using biyos::EventKind;
using biyos::EventLevel;
using biyos::Field;
using biyos::find_kind;
using biyos::find_level;
using biyos::Session;
using biyos::to_index;
using biyos::to_size;
using biyos::capi::guarded;
using biyos::capi::handle_of;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

struct Forwarder
{
    Env *env;
    BIYOS_hook hook;
    void *data;

    void operator()(const Event &event) const
    {
        const ErrorScope scope(*env);
        const BIYOS_event view{&event};
        hook(&view, data);
    }
};

template <typename List>
void add_to(List &list,
            Env &env,
            BIYOS_hook hook,
            void *data,
            BIYOS_int kind,
            BIYOS_int level,
            BIYOS_index *index)
{
    const Forwarder forwarder{&env, require(hook, "hk"), data};
    const EventKind found = find_kind(kind);
    const EventLevel lowest = find_level(level);
    BIYOS_index *id = require(index, "idx");
    *id = list.add_hook(found, lowest, forwarder);
}

void add_hook(Env &env,
              BIYOS_hook hook,
              void *data,
              BIYOS_int kind,
              BIYOS_int level,
              BIYOS_index *index)
{
    add_to(writable(env).events(), env, hook, data, kind, level, index);
}

void delete_hook(Env &env, BIYOS_index index)
{
    writable(env).events().delete_hook(index);
}

void add_model_hook(Session &session,
                    BIYOS_hook hook,
                    void *data,
                    BIYOS_int kind,
                    BIYOS_int level,
                    BIYOS_index *index)
{
    writable(session);
    add_to(session, session.env(), hook, data, kind, level, index);
}

void delete_model_hook(Session &session, BIYOS_index index)
{
    writable(session);
    session.delete_hook(index);
}

const Field *field_of(const BIYOS_event *event, BIYOS_index field)
{
    if (event == nullptr || field < 0)
    {
        return nullptr;
    }
    const std::vector<Field> &fields = event->core->fields;
    const auto index = to_size(field);
    return index < fields.size() ? &fields[index] : nullptr;
}

}

BIYOS_status BIYOS_addenvhook(BIYOS_env *env,
                              BIYOS_hook hk,
                              void *data,
                              BIYOS_int knd,
                              BIYOS_int lvl,
                              BIYOS_index *idx)
{
    return guarded<add_hook>(env, hk, data, knd, lvl, idx);
}

BIYOS_status BIYOS_delenvhook(BIYOS_env *env, BIYOS_index idx)
{
    return guarded<delete_hook>(env, idx);
}

BIYOS_status BIYOS_addmdlhook(BIYOS_model *mdl,
                              BIYOS_hook hk,
                              void *data,
                              BIYOS_int knd,
                              BIYOS_int lvl,
                              BIYOS_index *idx)
{
    return guarded<add_model_hook>(mdl, hk, data, knd, lvl, idx);
}

BIYOS_status BIYOS_delmdlhook(BIYOS_model *mdl, BIYOS_index idx)
{
    return guarded<delete_model_hook>(mdl, idx);
}

BIYOS_int BIYOS_getevtknd(const BIYOS_event *evt)
{
    return evt == nullptr ? 0 : static_cast<BIYOS_int>(evt->core->kind);
}

BIYOS_int BIYOS_getevtlvl(const BIYOS_event *evt)
{
    return evt == nullptr ? 0 : static_cast<BIYOS_int>(evt->core->level);
}

const char *BIYOS_getevtmod(const BIYOS_event *evt)
{
    return evt == nullptr ? "" : evt->core->module.c_str();
}

const char *BIYOS_getevttext(const BIYOS_event *evt)
{
    return evt == nullptr ? "" : evt->core->text.c_str();
}

BIYOS_index BIYOS_getnevtfld(const BIYOS_event *evt)
{
    return evt == nullptr ? 0 : to_index(evt->core->fields.size());
}

const char *BIYOS_getevtfldname(const BIYOS_event *evt, BIYOS_index idx)
{
    const Field *found = field_of(evt, idx);
    return found == nullptr ? "" : found->name.c_str();
}

const char *BIYOS_getevtfldval(const BIYOS_event *evt, BIYOS_index idx)
{
    const Field *found = field_of(evt, idx);
    return found == nullptr ? "" : found->value.c_str();
}

const BIYOS_model *BIYOS_getevtmdl(const BIYOS_event *evt)
{
    return evt == nullptr ? nullptr : handle_of(evt->core->model);
}
