#include "biyos/biyos.h"

#include "capi/entry.hpp"
#include "core/environment.hpp"

using biyos::capi::guarded;
using biyos::capi::require;
using biyos::capi::writable;

namespace
{

void create(biyos::Environment &env, biyos_model **model)
{
    biyos_model **made = require(model, "model");
    writable(env);
    *made = new biyos_model(env);
}

void freeable(biyos_model &model)
{
    writable(model);
}

}

biyos_status biyos_model_create(biyos_env *env, biyos_model **model)
{
    if (model != nullptr)
    {
        *model = nullptr;
    }
    return guarded<create>(env, model);
}

biyos_status biyos_model_free(biyos_model *model)
{
    if (model == nullptr)
    {
        return BIYOS_OK;
    }
    const biyos_status status = guarded<freeable>(model);
    if (status == BIYOS_OK)
    {
        delete model;
    }
    return status;
}
