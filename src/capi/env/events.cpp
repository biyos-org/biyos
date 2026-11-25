#include "biyos/biyos.h"

#include "capi/env/event.hpp"
#include "capi/utils/entry.hpp"
#include "core/env.hpp"
#include "core/events/events.hpp"

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
        const biyos::ErrorScope scope(*env);
        const BIYOS_event view{&event};
        hook(&view, data);
    }
};

template <typename List>
void add_to(List &list,
            biyos::Env &env,
            BIYOS_hook hook,
            void *data,
            int kind,
            int level,
            int *index)
{
    const Forwarder forwarder{&env, require(hook, "hk"), data};
    const biyos::EventKind found = biyos::find_kind(kind);
    const biyos::EventLevel lowest = biyos::find_level(level);
    int *id = require(index, "idx");
    *id = list.add_hook(found, lowest, forwarder);
}

void add_hook(biyos::Env &env, BIYOS_hook hook, void *data, int kind, int level, int *index)
{
    add_to(writable(env).events(), env, hook, data, kind, level, index);
}

void delete_hook(biyos::Env &env, int index)
{
    writable(env).events().delete_hook(index);
}

void add_model_hook(BIYOS_model &model,
                    BIYOS_hook hook,
                    void *data,
                    int kind,
                    int level,
                    int *index)
{
    writable(model);
    add_to(model.hooks, model.handle.env(), hook, data, kind, level, index);
}

void delete_model_hook(BIYOS_model &model, int index)
{
    writable(model);
    model.hooks.delete_hook(index);
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

BIYOS_index BIYOS_getnevtfld(const BIYOS_event *evt)
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

const BIYOS_model *BIYOS_getevtmdl(const BIYOS_event *evt)
{
    return evt == nullptr ? nullptr : evt->core->model;
}
