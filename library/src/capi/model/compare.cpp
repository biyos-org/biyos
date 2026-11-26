#include "biyos/biyos.h"

#include "capi/utils/busy.hpp"
#include "capi/utils/entry.hpp"
#include "core/timing/timer.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"
#include "model/session/session.hpp"

using biyos::compare;
using biyos::EntryTimer;
using biyos::kModule;
using biyos::Session;
using biyos::capi::guarded;
using biyos::capi::ModelAccess;
using biyos::capi::require;

namespace
{

void compare_models(const Session &session, const BIYOS_model *other, BIYOS_int *same)
{
    const BIYOS_model &b = *require(other, "b");
    BIYOS_int *result = require(same, "same");
    const ModelAccess access(b.env(), b.usage());
    access.check();
    const EntryTimer timer(session.stream(), kModule, "compare");
    *result = compare(session.stream(), session.model(), b.model()) ? 1 : 0;
}

}

BIYOS_status BIYOS_cmpmdl(const BIYOS_model *a, const BIYOS_model *b, BIYOS_int *same)
{
    return guarded<compare_models>(a, b, same);
}
