#include "biyos/biyos.h"

#include "capi/evaluation/cache.hpp"
#include "capi/utils/arrays.hpp"
#include "capi/utils/entry.hpp"
#include "core/numbers.hpp"
#include "core/options/lookup.hpp"
#include "model/evaluation/evaluation.hpp"

#include <cstddef>

using biyos::Evaluation;
using biyos::Session;
using biyos::to_size;
using biyos::capi::evaluation_of;
using biyos::capi::guarded;
using biyos::capi::Running;
using biyos::capi::span_of;

namespace
{

std::size_t n_vars(const Session &session)
{
    return to_size(session.model().n_variables());
}

std::size_t n_funs(const Session &session)
{
    return to_size(session.model().n_functions());
}

void eval_f(Session &session, const BIYOS_real *x, BIYOS_real *f)
{
    Evaluation &evaluation = evaluation_of(session);
    const auto point = span_of(x, n_vars(session), "x");
    const auto values = span_of(f, n_funs(session), "f");
    const Running running(session);
    evaluation.values(session.lookup(), point, values);
}

void eval_g(Session &session, const BIYOS_real *x, BIYOS_real *g)
{
    Evaluation &evaluation = evaluation_of(session);
    const std::size_t nnz = evaluation.jacobian_pattern().variables.size();
    const auto point = span_of(x, n_vars(session), "x");
    const auto values = span_of(g, nnz, "g");
    const Running running(session);
    evaluation.jacobian(session.lookup(), point, values);
}

void eval_h(Session &session, const BIYOS_real *x, const BIYOS_real *w, BIYOS_real *h)
{
    Evaluation &evaluation = evaluation_of(session);
    const std::size_t nnz = evaluation.hessian_pattern().rows.size();
    const auto point = span_of(x, n_vars(session), "x");
    const auto weights = span_of(w, n_funs(session), "w");
    const auto values = span_of(h, nnz, "h");
    const Running running(session);
    evaluation.hessian(session.lookup(), {.x = point, .w = weights, .v = {}}, values);
}

void eval_hv(Session &session,
             const BIYOS_real *x,
             const BIYOS_real *w,
             const BIYOS_real *v,
             BIYOS_real *hv)
{
    Evaluation &evaluation = evaluation_of(session);
    const auto point = span_of(x, n_vars(session), "x");
    const auto weights = span_of(w, n_funs(session), "w");
    const auto direction = span_of(v, n_vars(session), "v");
    const auto values = span_of(hv, n_vars(session), "hv");
    const Running running(session);
    evaluation.product(session.lookup(), {.x = point, .w = weights, .v = direction}, values);
}

}

BIYOS_status BIYOS_evalf(BIYOS_model *mdl, const BIYOS_real *x, BIYOS_real *f)
{
    return guarded<eval_f>(mdl, x, f);
}

BIYOS_status BIYOS_evalg(BIYOS_model *mdl, const BIYOS_real *x, BIYOS_real *g)
{
    return guarded<eval_g>(mdl, x, g);
}

BIYOS_status BIYOS_evalh(BIYOS_model *mdl, const BIYOS_real *x, const BIYOS_real *w, BIYOS_real *h)
{
    return guarded<eval_h>(mdl, x, w, h);
}

BIYOS_status BIYOS_evalhv(BIYOS_model *mdl,
                          const BIYOS_real *x,
                          const BIYOS_real *w,
                          const BIYOS_real *v,
                          BIYOS_real *hv)
{
    return guarded<eval_hv>(mdl, x, w, v, hv);
}
