#include "biyos/events.h"
#include "biyos/expr.h"
#include "biyos/model.h"
#include "biyos/options.h"
#include "biyos/status.h"

#include "biyos/biyos.h"

#include <stddef.h>

extern char index_size[sizeof(BIYOS_index) == 4 ? 1 : -1];
extern char int_size[sizeof(BIYOS_int) == 4 ? 1 : -1];
extern char offset_size[sizeof(BIYOS_offset) == 8 ? 1 : -1];
extern char real_size[sizeof(BIYOS_real) == 8 ? 1 : -1];

#if !defined(BIYOS_OK) || BIYOS_OK != 0
#error "BIYOS_OK must be zero"
#endif
#if BIYOS_ERR_INVALIDARG > -100 || BIYOS_ERR_INVALIDARG < -199
#error "BIYOS_ERR_INVALIDARG must be a caller error"
#endif
#if BIYOS_ERR_NOMEMORY > -400 || BIYOS_ERR_NOMEMORY < -499
#error "BIYOS_ERR_NOMEMORY must be a resource error"
#endif
#if BIYOS_ERR_INTERNAL > -500 || BIYOS_ERR_INTERNAL < -599
#error "BIYOS_ERR_INTERNAL must be an internal error"
#endif
#if BIYOS_OPT_SEED != 1 || BIYOS_OPT_MAXTIME != 2 || BIYOS_OPT_CHECKLVL != 3 ||                    \
    BIYOS_OPT_PRESOLVE != 4 || BIYOS_OPT_REPRO != 5 || BIYOS_OPT_FEASTOLABS != 6 ||                \
    BIYOS_OPT_FEASTOLREL != 7 || BIYOS_OPT_INTTOL != 8 || BIYOS_OPT_ZEROTOL != 9 ||                \
    BIYOS_OPT_MAXCPUTIME != 10
#error "a public option id changed"
#endif
#if BIYOS_KIND_BOOL != 1 || BIYOS_KIND_INT != 2 || BIYOS_KIND_DBL != 3 || BIYOS_KIND_STR != 4 ||   \
    BIYOS_KIND_ENUM != 5
#error "a public option kind changed"
#endif
#if BIYOS_EVT_MESSAGE != 1 || BIYOS_EVT_CHECK != 2 || BIYOS_EVT_PHASE != 3
#error "a public event kind changed"
#endif
#if BIYOS_LVL_DBG != 1 || BIYOS_LVL_INFO != 2 || BIYOS_LVL_WARN != 3 || BIYOS_LVL_ERR != 4
#error "a public level changed"
#endif

#if !defined(BIYOS_TERM_NOTSOLVED) || BIYOS_TERM_NOTSOLVED != 0 || BIYOS_TERM_OPTIMAL != 101 ||    \
    BIYOS_TERM_LOCALLYSOLVED != 102 || BIYOS_TERM_ALMOSTLOCALLYSOLVED != 103 ||                    \
    BIYOS_TERM_ITERLIMIT != 201 || BIYOS_TERM_TIMELIMIT != 202 ||                                  \
    BIYOS_TERM_CPUTIMELIMIT != 203 || BIYOS_TERM_EVALLIMIT != 204 || BIYOS_TERM_USERSTOP != 205 || \
    BIYOS_TERM_INFEASIBLE != 301 || BIYOS_TERM_LOCALLYINFEASIBLE != 302 ||                         \
    BIYOS_TERM_UNBOUNDED != 303 || BIYOS_TERM_SLOWPROGRESS != 401 ||                               \
    BIYOS_TERM_NUMERICALTROUBLE != 402
#error "a public termination changed"
#endif
#if !defined(BIYOS_RES_NONE) || BIYOS_RES_NONE != 0 || BIYOS_RES_FEASIBLE != 1 ||                  \
    BIYOS_RES_NEARLYFEASIBLE != 2 || BIYOS_RES_INFEASIBLE != 3 || BIYOS_RES_CERTIFICATE != 4 ||    \
    BIYOS_RES_NEARLYCERTIFICATE != 5 || BIYOS_RES_UNKNOWN != 6
#error "a public result status changed"
#endif
#if !defined(BIYOS_CLAIM_NONE) || BIYOS_CLAIM_NONE != 0 || BIYOS_CLAIM_LOCAL != 1 ||               \
    BIYOS_CLAIM_GLOBAL != 2
#error "a public claim changed"
#endif
#if !defined(BIYOS_VAR_CONTINUOUS) || BIYOS_VAR_CONTINUOUS != 1 || BIYOS_VAR_BINARY != 2 ||        \
    BIYOS_VAR_INTEGER != 3 || BIYOS_VAR_SEMICONTINUOUS != 4 || BIYOS_VAR_SEMIINTEGER != 5
#error "a public variable kind changed"
#endif
#if !defined(BIYOS_MINIMIZE) || BIYOS_MINIMIZE != 1 || BIYOS_MAXIMIZE != 2
#error "a public objective sense changed"
#endif
#if !defined(BIYOS_OP_CONST) || BIYOS_OP_CONST != 1 || BIYOS_OP_VAR != 2 || BIYOS_OP_SUM != 3 ||   \
    BIYOS_OP_PROD != 4 || BIYOS_OP_NEG != 5 || BIYOS_OP_DIV != 6 || BIYOS_OP_CPOW != 7 ||          \
    BIYOS_OP_POW != 8 || BIYOS_OP_EXP != 9 || BIYOS_OP_LOG != 10 || BIYOS_OP_SQRT != 11 ||         \
    BIYOS_OP_SIN != 12 || BIYOS_OP_COS != 13 || BIYOS_OP_TAN != 14 || BIYOS_OP_ABS != 15 ||        \
    BIYOS_OP_MAX != 16 || BIYOS_OP_MIN != 17 || BIYOS_OP_ASIN != 18 || BIYOS_OP_ACOS != 19 ||      \
    BIYOS_OP_ATAN != 20 || BIYOS_OP_SINH != 21 || BIYOS_OP_COSH != 22 || BIYOS_OP_TANH != 23 ||    \
    BIYOS_OP_ASINH != 24 || BIYOS_OP_ACOSH != 25 || BIYOS_OP_ATANH != 26 || BIYOS_OP_ENT != 27 ||  \
    BIYOS_OP_SPOW != 28 || BIYOS_OP_ERF != 29
#error "a public operator changed"
#endif

static void count(const BIYOS_event *event, void *data)
{
    int *errors = data;
    if (BIYOS_getevtlvl(event) == BIYOS_LVL_ERR)
    {
        ++*errors;
    }
}

static BIYOS_status use_functions(BIYOS_model *model)
{
    const double constants[2] = {1.0, 2.0};
    const int64_t starts[3] = {0, 2, 3};
    const int variables[3] = {1, 0, 1};
    const double values[3] = {1.0, 2.0, 3.0};
    int64_t offsets[3] = {-1, -1, -1};
    BIYOS_offset needed = -1;
    int held[3] = {-1, -1, -1};
    double kept[3] = {0.0, 0.0, 0.0};
    BIYOS_index first = -1;
    BIYOS_index functions = 0;
    if (BIYOS_addfuns(model, 2, constants, starts, variables, values, &first) != BIYOS_OK ||
        first != 0 || BIYOS_getnfuns(model, &functions) != BIYOS_OK || functions != 2)
    {
        return BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_getfuns(model, 0, 2, NULL, offsets, 3, &needed, held, kept) != BIYOS_OK ||
        needed != 3 || offsets[1] != 2 || held[0] != 0 || held[1] != 1)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_quadratic(BIYOS_model *model)
{
    const int functions[1] = {1};
    const int64_t starts[2] = {0, 2};
    const int firsts[2] = {1, 0};
    const int seconds[2] = {0, 0};
    const double values[2] = {2.0, 1.0};
    int64_t offsets[3] = {-1, -1, -1};
    BIYOS_offset needed = -1;
    int held[2] = {-1, -1};
    int paired[2] = {-1, -1};
    BIYOS_status status = BIYOS_addquadterms(model, 1, functions, starts, firsts, seconds, values);
    if (status == BIYOS_OK)
    {
        status = BIYOS_getquadterms(model, 0, 2, offsets, 2, &needed, held, paired, NULL);
    }
    if (status != BIYOS_OK || needed != 2 || offsets[1] != 0 || offsets[2] != 2 || held[1] != 0 ||
        paired[1] != 1)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_constraints(BIYOS_model *model)
{
    const int functions[2] = {1, 1};
    const double lower[2] = {0.0, -1.0};
    const int indices[1] = {1};
    int found[2] = {-1, -1};
    BIYOS_index first = -1;
    BIYOS_index constraints = 0;
    if (BIYOS_addcons(model, 2, functions, lower, NULL, &first) != BIYOS_OK || first != 0 ||
        BIYOS_setconbounds(model, 1, indices, NULL, lower) != BIYOS_OK ||
        BIYOS_getncons(model, &constraints) != BIYOS_OK || constraints != 2 ||
        BIYOS_getcons(model, 0, 2, found, NULL, NULL) != BIYOS_OK || found[1] != 1)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_objectives(BIYOS_model *model)
{
    const int functions[2] = {0, 1};
    const int senses[2] = {BIYOS_MAXIMIZE, BIYOS_MINIMIZE};
    const double weights[1] = {2.0};
    const int indices[1] = {1};
    int found[2] = {-1, -1};
    int priorities[2] = {-1, -1};
    BIYOS_index first = -1;
    BIYOS_index objectives = 0;
    BIYOS_status status =
        BIYOS_addobjs(model, 2, functions, senses, NULL, NULL, NULL, NULL, &first);
    if (status == BIYOS_OK)
    {
        status = BIYOS_setobjs(model, 1, indices, NULL, NULL, weights, NULL, NULL);
    }
    if (status == BIYOS_OK)
    {
        status = BIYOS_getnobjs(model, &objectives);
    }
    if (status == BIYOS_OK)
    {
        status = BIYOS_getobjs(model, 0, 2, NULL, found, priorities, NULL, NULL, NULL);
    }
    if (status != BIYOS_OK || first != 0 || objectives != 2 || found[0] != BIYOS_MAXIMIZE ||
        found[1] != BIYOS_MINIMIZE || priorities[1] != 1)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_copy(BIYOS_env *env, BIYOS_model *model)
{
    BIYOS_model *copy = NULL;
    BIYOS_index constraints = 0;
    BIYOS_status status = BIYOS_copymodel(model, env, &copy);
    if (status == BIYOS_OK && (BIYOS_getncons(copy, &constraints) != BIYOS_OK || constraints != 2))
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_freemodel(copy) != BIYOS_OK)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    return status;
}

int main(void)
{
    BIYOS_status status = BIYOS_OK;
    BIYOS_env *env = NULL;
    BIYOS_model *model = NULL;
    BIYOS_int major = -1;
    BIYOS_int minor = -1;
    BIYOS_int patch = -1;
    BIYOS_int seed = -1;
    BIYOS_int kind = 0;
    int lowest = BIYOS_LVL_DBG;
    int errors = 0;
    BIYOS_index id = 0;
    const int kinds[2] = {BIYOS_VAR_CONTINUOUS, BIYOS_VAR_BINARY};
    const double lower[2] = {-1.0, 0.0};
    const double upper[2] = {1.0, 1.0};
    const int indices[1] = {0};
    int found[2] = {0, 0};
    BIYOS_index first = -1;
    BIYOS_index variables = 0;
    BIYOS_version(&major, &minor, &patch);
    if (major < 0 || minor < 0 || patch < 0)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_newenv(&env) != BIYOS_OK || BIYOS_lasterror(env)[0] != '\0')
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_setintopt(env, BIYOS_OPT_SEED, 3) != BIYOS_OK ||
        BIYOS_getintopt(env, BIYOS_OPT_SEED, &seed) != BIYOS_OK || seed != 3)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_getoptkind(env, BIYOS_OPT_SEED, &kind) != BIYOS_OK || kind != BIYOS_KIND_INT)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_addhook(env, count, &errors, BIYOS_EVT_MESSAGE, lowest, &id) != BIYOS_OK ||
        BIYOS_setintopt(env, BIYOS_OPT_SEED, -1) != BIYOS_ERR_INVALIDARG ||
        BIYOS_delhook(env, id) != BIYOS_OK || errors != 1)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_newmodel(env, &model) != BIYOS_OK ||
        BIYOS_addvars(model, 2, kinds, lower, upper, &first) != BIYOS_OK || first != 0 ||
        BIYOS_setvarbounds(model, 1, indices, lower + 1, NULL) != BIYOS_OK ||
        BIYOS_setvarkinds(model, 1, indices, kinds + 1) != BIYOS_OK ||
        BIYOS_getnvars(model, &variables) != BIYOS_OK || variables != 2 ||
        BIYOS_getvars(model, 0, 2, found, NULL, NULL) != BIYOS_OK || found[0] != BIYOS_VAR_BINARY ||
        found[1] != BIYOS_VAR_BINARY)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (use_functions(model) != BIYOS_OK || use_quadratic(model) != BIYOS_OK ||
        use_constraints(model) != BIYOS_OK || use_objectives(model) != BIYOS_OK ||
        use_copy(env, model) != BIYOS_OK)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_freeenv(env) == BIYOS_OK || BIYOS_freemodel(model) != BIYOS_OK)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_freeenv(env) != BIYOS_OK)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    return status == BIYOS_OK ? 0 : 1;
}
