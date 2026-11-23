#include "biyos/biyos.h"

#include <stddef.h>
#include <string.h>

extern char index_size[sizeof(BIYOS_index) == 4 ? 1 : -1];
extern char int_size[sizeof(BIYOS_int) == 4 ? 1 : -1];
extern char offset_size[sizeof(BIYOS_offset) == 8 ? 1 : -1];
extern char real_size[sizeof(BIYOS_real) == 8 ? 1 : -1];
extern char mask_size[sizeof(BIYOS_mask) == 8 ? 1 : -1];
extern char mask_sign[(BIYOS_mask)-1 > 0 ? 1 : -1];
extern char error_size[offsetof(BIYOS_error, size) == 0 ? 1 : -1];
extern char error_status[offsetof(BIYOS_error, status) == sizeof(size_t) ? 1 : -1];
extern char error_mdl[offsetof(BIYOS_error, mdl) == 2 * sizeof(void *) ? 1 : -1];
extern char error_msg[offsetof(BIYOS_error, msg) == 3 * sizeof(void *) ? 1 : -1];
extern char error_end[sizeof(BIYOS_error) == 4 * sizeof(void *) ? 1 : -1];
extern char request_size[offsetof(BIYOS_cbreq, size) == 0 ? 1 : -1];
extern char request_mdl[offsetof(BIYOS_cbreq, mdl) == sizeof(size_t) ? 1 : -1];
extern char request_cb[offsetof(BIYOS_cbreq, cb) == 2 * sizeof(void *) ? 1 : -1];
extern char request_thread[offsetof(BIYOS_cbreq, thread) == 2 * sizeof(void *) + 4 ? 1 : -1];
extern char request_x[offsetof(BIYOS_cbreq, x) == 2 * sizeof(void *) + 8 ? 1 : -1];
extern char request_w[offsetof(BIYOS_cbreq, w) == 3 * sizeof(void *) + 8 ? 1 : -1];
extern char request_v[offsetof(BIYOS_cbreq, v) == 4 * sizeof(void *) + 8 ? 1 : -1];
extern char request_end[sizeof(BIYOS_cbreq) == 5 * sizeof(void *) + 8 ? 1 : -1];

#if !defined(BIYOS_OK) || BIYOS_OK != 0
#error "BIYOS_OK must be zero"
#endif
#if BIYOS_ERR_BADARG > -100 || BIYOS_ERR_BADARG < -199
#error "BIYOS_ERR_BADARG must be a caller error"
#endif
#if BIYOS_ERR_BUSY > -200 || BIYOS_ERR_BUSY < -299
#error "BIYOS_ERR_BUSY must be a model error"
#endif
#if BIYOS_ERR_INUSE > -200 || BIYOS_ERR_INUSE < -299
#error "BIYOS_ERR_INUSE must be a model error"
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
#if BIYOS_KND_BOOL != 1 || BIYOS_KND_INT != 2 || BIYOS_KND_DBL != 3 || BIYOS_KND_STR != 4 ||       \
    BIYOS_KND_ENUM != 5 || BIYOS_KND_FLG != 6
#error "a public option kind changed"
#endif
#if !defined(BIYOS_OPTSRC_MDL) || BIYOS_OPTSRC_MDL != 1 || BIYOS_OPTSRC_ENV != 2 ||                \
    BIYOS_OPTSRC_DEFAULT != 3
#error "a public option source changed"
#endif
#if !defined(BIYOS_CHECKLVL_NONE) || BIYOS_CHECKLVL_NONE != 1 || BIYOS_CHECKLVL_CHEAP != 2 ||      \
    BIYOS_CHECKLVL_FULL != 3
#error "a public check level changed"
#endif
#if BIYOS_EVT_MESSAGE != 1 || BIYOS_EVT_CHECK != 2 || BIYOS_EVT_PHASE != 3
#error "a public event kind changed"
#endif
#if BIYOS_LVL_DBG != 1 || BIYOS_LVL_INFO != 2 || BIYOS_LVL_WARN != 3 || BIYOS_LVL_ERR != 4
#error "a public level changed"
#endif

#if !defined(BIYOS_TRM_NONE) || BIYOS_TRM_NONE != 0 || BIYOS_TRM_OPTIMAL != 101 ||                 \
    BIYOS_TRM_LOCAL != 102 || BIYOS_TRM_NEARLOCAL != 103 || BIYOS_TRM_ITERLIM != 201 ||            \
    BIYOS_TRM_TIMELIM != 202 || BIYOS_TRM_CPULIM != 203 || BIYOS_TRM_EVALLIM != 204 ||             \
    BIYOS_TRM_STOP != 205 || BIYOS_TRM_INFEAS != 301 || BIYOS_TRM_LOCINFEAS != 302 ||              \
    BIYOS_TRM_UNBND != 303 || BIYOS_TRM_SLOW != 401 || BIYOS_TRM_NUMERIC != 402
#error "a public termination changed"
#endif
#if !defined(BIYOS_RES_NONE) || BIYOS_RES_NONE != 0 || BIYOS_RES_FEAS != 1 ||                      \
    BIYOS_RES_NEARFEAS != 2 || BIYOS_RES_INFEAS != 3 || BIYOS_RES_CER != 4 ||                      \
    BIYOS_RES_NEARCER != 5 || BIYOS_RES_UNKNOWN != 6
#error "a public result status changed"
#endif
#if !defined(BIYOS_CLM_NONE) || BIYOS_CLM_NONE != 0 || BIYOS_CLM_LOCAL != 1 || BIYOS_CLM_GLOBAL != 2
#error "a public claim changed"
#endif
#if !defined(BIYOS_VAR_CONT) || BIYOS_VAR_CONT != 1 || BIYOS_VAR_BIN != 2 || BIYOS_VAR_INT != 3 || \
    BIYOS_VAR_SEMICONT != 4 || BIYOS_VAR_SEMIINT != 5
#error "a public variable kind changed"
#endif
#if !defined(BIYOS_MINIMIZE) || BIYOS_MINIMIZE != 1 || BIYOS_MAXIMIZE != 2
#error "a public objective sense changed"
#endif
#if !defined(BIYOS_SOSKND_SOS1) || BIYOS_SOSKND_SOS1 != 1 || BIYOS_SOSKND_SOS2 != 2
#error "a public set kind changed"
#endif
#if !defined(BIYOS_CONE_NONNEG) || BIYOS_CONE_NONNEG != 1 || BIYOS_CONE_SOC != 2 ||                \
    BIYOS_CONE_RSOC != 3 || BIYOS_CONE_EXP != 4 || BIYOS_CONE_DEXP != 5 || BIYOS_CONE_POW != 6 ||  \
    BIYOS_CONE_DPOW != 7 || BIYOS_CONE_PSD != 8
#error "a public cone kind changed"
#endif
#if !defined(BIYOS_OP_CONST) || BIYOS_OP_CONST != 1 || BIYOS_OP_VAR != 2 || BIYOS_OP_SUM != 3 ||   \
    BIYOS_OP_PROD != 4 || BIYOS_OP_NEG != 5 || BIYOS_OP_DIV != 6 || BIYOS_OP_CPOW != 7 ||          \
    BIYOS_OP_POW != 8 || BIYOS_OP_EXP != 9 || BIYOS_OP_LOG != 10 || BIYOS_OP_SQRT != 11 ||         \
    BIYOS_OP_SIN != 12 || BIYOS_OP_COS != 13 || BIYOS_OP_TAN != 14 || BIYOS_OP_ABS != 15 ||        \
    BIYOS_OP_MAX != 16 || BIYOS_OP_MIN != 17 || BIYOS_OP_ASIN != 18 || BIYOS_OP_ACOS != 19 ||      \
    BIYOS_OP_ATAN != 20 || BIYOS_OP_SINH != 21 || BIYOS_OP_COSH != 22 || BIYOS_OP_TANH != 23 ||    \
    BIYOS_OP_ASINH != 24 || BIYOS_OP_ACOSH != 25 || BIYOS_OP_ATANH != 26 || BIYOS_OP_ENT != 27 ||  \
    BIYOS_OP_SPOW != 28 || BIYOS_OP_ERF != 29 || BIYOS_OP_NODE != 30 || BIYOS_OP_KEEP != 32 ||     \
    BIYOS_OP_REF != 33
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
    int held[3] = {-1, -1, -1};
    double kept[3] = {0.0, 0.0, 0.0};
    BIYOS_index first = -1;
    BIYOS_index functions = 0;
    if (BIYOS_addfun(model, 2, &first) != BIYOS_OK || first != 0 ||
        BIYOS_getnfun(model, &functions) != BIYOS_OK || functions != 2 ||
        BIYOS_setcstrg(model, 0, 2, constants) != BIYOS_OK ||
        BIYOS_addrlintermrg(model, 0, 2, starts, variables, values) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_getrlintermrg(model, 0, 2, 3, offsets, held, kept) != BIYOS_OK || offsets[1] != 2 ||
        offsets[2] != 3 || held[0] != 0 || held[1] != 1)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_linear(BIYOS_model *model)
{
    const int functions[1] = {1};
    const int variables[1] = {0};
    const double values[1] = {4.0};
    int64_t offsets[3] = {-1, -1, -1};
    BIYOS_offset needed = -1;
    int held[4] = {-1, -1, -1, -1};
    if (BIYOS_addlintermsp(model, 1, functions, variables, values) != BIYOS_OK ||
        BIYOS_getrlintermrg(model, 0, 2, 4, offsets, held, NULL) != BIYOS_OK || offsets[2] != 4 ||
        held[2] != 0 || held[3] != 1)
    {
        return BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_getnclintermrg(model, 1, 1, &needed) != BIYOS_OK || needed != 2 ||
        BIYOS_getclintermrg(model, 1, 1, 2, offsets, held, NULL) != BIYOS_OK || offsets[1] != 2 ||
        held[0] != 0 || held[1] != 1)
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
    int owners[2] = {-1, -1};
    int held[2] = {-1, -1};
    int paired[2] = {-1, -1};
    BIYOS_status status = BIYOS_addquadtermrg(model, 1, 1, starts, firsts, seconds, values);
    if (status == BIYOS_OK)
    {
        status = BIYOS_getquadtermrg(model, 0, 2, 2, offsets, held, paired, NULL);
    }
    if (status == BIYOS_OK)
    {
        status = BIYOS_getquadtermsp(model, 1, functions, 2, &needed, owners, NULL, NULL, NULL);
    }
    if (status != BIYOS_OK || needed != 2 || offsets[1] != 0 || offsets[2] != 2 || held[1] != 0 ||
        paired[1] != 1 || owners[1] != 1)
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
    int map[2] = {-1, -1};
    BIYOS_index first = -1;
    BIYOS_index constraints = 0;
    if (BIYOS_addcon(model, 2, functions, lower, NULL, &first) != BIYOS_OK || first != 0 ||
        BIYOS_setconbndsp(model, 1, indices, NULL, lower) != BIYOS_OK ||
        BIYOS_getncon(model, &constraints) != BIYOS_OK || constraints != 2 ||
        BIYOS_getconrg(model, 0, 2, found, NULL, NULL) != BIYOS_OK || found[1] != 1 ||
        BIYOS_delconsp(model, 0, NULL, map) != BIYOS_OK || map[1] != 1 ||
        BIYOS_delconrg(model, 2, 0, NULL) != BIYOS_OK)
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
    int map[2] = {-1, -1};
    BIYOS_index first = -1;
    BIYOS_index objectives = 0;
    BIYOS_status status = BIYOS_addobj(model, 2, functions, senses, NULL, NULL, &first);
    if (status == BIYOS_OK)
    {
        status = BIYOS_setobjcoefsp(model, 1, indices, weights);
    }
    if (status == BIYOS_OK)
    {
        status = BIYOS_setobjpriosp(model, 1, indices, indices);
    }
    if (status == BIYOS_OK)
    {
        status = BIYOS_getobjpriosp(model, 1, indices, priorities);
    }
    if (status == BIYOS_OK && priorities[0] != 1)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (status == BIYOS_OK)
    {
        status = BIYOS_getnobj(model, &objectives);
    }
    if (status == BIYOS_OK)
    {
        status = BIYOS_getobjrg(model, 0, 2, NULL, found, priorities, NULL);
    }
    if (status != BIYOS_OK || first != 0 || objectives != 2 || found[0] != BIYOS_MAXIMIZE ||
        found[1] != BIYOS_MINIMIZE || priorities[1] != 1 ||
        BIYOS_delobjsp(model, 0, NULL, map) != BIYOS_OK || map[1] != 1 ||
        BIYOS_delobjrg(model, 2, 0, NULL) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_indicators(BIYOS_model *model)
{
    const int variables[2] = {1, 0};
    const int values[2] = {1, 0};
    const int functions[2] = {0, 1};
    const double upper[1] = {4.0};
    const int indices[1] = {1};
    const int continuous = BIYOS_VAR_CONT;
    int found[2] = {-1, -1};
    int map[2] = {-1, -1};
    BIYOS_index first = -1;
    BIYOS_index indicators = 0;
    if (BIYOS_addind(model, 2, variables, values, functions, NULL, NULL, &first) != BIYOS_OK ||
        first != 0 || BIYOS_setindbndsp(model, 1, indices, NULL, upper) != BIYOS_OK ||
        BIYOS_setindvarrg(model, 1, 1, variables, values) != BIYOS_OK ||
        BIYOS_getnind(model, &indicators) != BIYOS_OK || indicators != 2 ||
        BIYOS_getindrg(model, 0, 2, found, NULL, NULL, NULL, NULL) != BIYOS_OK || found[0] != 1 ||
        found[1] != 1 || BIYOS_setvarkndsp(model, 1, indices, &continuous) != BIYOS_ERR_INUSE ||
        BIYOS_delindsp(model, 0, NULL, map) != BIYOS_OK || map[1] != 1 ||
        BIYOS_delindrg(model, 2, 0, NULL) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_sos(BIYOS_model *model)
{
    const int kinds[2] = {BIYOS_SOSKND_SOS1, BIYOS_SOSKND_SOS2};
    const BIYOS_offset starts[3] = {0, 2, 3};
    const int variables[3] = {1, 0, 1};
    const int indices[1] = {1};
    int found[2] = {-1, -1};
    BIYOS_offset offsets[3] = {-1, -1, -1};
    int members[3] = {-1, -1, -1};
    double weights[3] = {0.0, 0.0, 0.0};
    int map[2] = {-1, -1};
    BIYOS_offset needed = -1;
    BIYOS_index first = -1;
    BIYOS_index sos = 0;
    if (BIYOS_addsos(model, 2, kinds, starts, variables, NULL, &first) != BIYOS_OK || first != 0 ||
        BIYOS_getnsos(model, &sos) != BIYOS_OK || sos != 2 ||
        BIYOS_getnsosvarsp(model, 1, indices, &needed) != BIYOS_OK || needed != 1 ||
        BIYOS_getnsosvarrg(model, 0, 2, &needed) != BIYOS_OK || needed != 3)
    {
        return BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_getsosrg(model, 0, 2, 3, found, offsets, members, weights) != BIYOS_OK ||
        found[1] != BIYOS_SOSKND_SOS2 || offsets[2] != 3 || members[1] != 0 ||
        BIYOS_getsossp(model, 1, indices, 1, found, offsets, members, NULL) != BIYOS_OK ||
        found[0] != BIYOS_SOSKND_SOS2 || offsets[1] != 1 || members[0] != 1 ||
        BIYOS_delsossp(model, 0, NULL, map) != BIYOS_OK || map[1] != 1 ||
        BIYOS_delsosrg(model, 2, 0, NULL) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_cones(BIYOS_model *model)
{
    const int kinds[2] = {BIYOS_CONE_SOC, BIYOS_CONE_POW};
    const int64_t offsets[3] = {0, 2, 4};
    const int functions[4] = {1, 0, 0, 1};
    const int64_t starts[3] = {0, 0, 1};
    const double weights[1] = {0.5};
    const int indices[1] = {1};
    int64_t read[3] = {-1, -1, -1};
    int members[4] = {-1, -1, -1, -1};
    int found[2] = {0, 0};
    int map[2] = {-1, -1};
    double weight = 0.0;
    BIYOS_index first = -1;
    BIYOS_index cones = 0;
    int64_t needed = -1;
    if (BIYOS_addcone(model, 2, kinds, offsets, functions, starts, weights, &first) != BIYOS_OK ||
        BIYOS_getncone(model, &cones) != BIYOS_OK || first != 0 || cones != 2 ||
        BIYOS_getnconefunrg(model, 0, 2, &needed) != BIYOS_OK || needed != 4 ||
        BIYOS_getconerg(model, 0, 2, 4, found, read, members) != BIYOS_OK ||
        found[1] != BIYOS_CONE_POW || read[1] != 2 || members[0] != 1)
    {
        return BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_getnconeprmsp(model, 1, indices, &needed) != BIYOS_OK || needed != 1 ||
        BIYOS_getconeprmsp(model, 1, indices, 1, read, &weight) != BIYOS_OK || read[1] != 1 ||
        memcmp(&weight, weights, sizeof weight) != 0 ||
        BIYOS_getnconefunsp(model, 0, NULL, &needed) != BIYOS_OK ||
        BIYOS_getconesp(model, 0, NULL, 0, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getnconeprmrg(model, 0, 0, &needed) != BIYOS_OK ||
        BIYOS_getconeprmrg(model, 0, 0, 0, NULL, NULL) != BIYOS_OK ||
        BIYOS_delconesp(model, 0, NULL, map) != BIYOS_OK || map[1] != 1 ||
        BIYOS_delconerg(model, 2, 0, NULL) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_complements(BIYOS_model *model)
{
    const int functions[2] = {0, 1};
    const int multipliers[2] = {1, 1};
    const double lower[1] = {-1.0};
    const double tolerance[1] = {0.5};
    const int indices[1] = {1};
    double found[2] = {9.0, 9.0};
    int read[2] = {-1, -1};
    int map[2] = {-1, -1};
    BIYOS_index first = -1;
    BIYOS_index complements = 0;
    if (BIYOS_addccon(model, 2, functions, multipliers, NULL, NULL, &first) != BIYOS_OK ||
        first != 0 || BIYOS_setcconbndsp(model, 1, indices, lower, NULL) != BIYOS_OK ||
        BIYOS_getnccon(model, &complements) != BIYOS_OK || complements != 2 ||
        BIYOS_getcconbndrg(model, 0, 2, found, NULL) != BIYOS_OK ||
        BIYOS_getcconrg(model, 0, 2, read, NULL, NULL, NULL) != BIYOS_OK || read[0] != 0 ||
        read[1] != 1 || BIYOS_setccontolrg(model, 1, 1, tolerance, NULL) != BIYOS_OK ||
        BIYOS_getccontolsp(model, 1, indices, found, found + 1) != BIYOS_OK ||
        BIYOS_delccontolsp(model, 1, indices) != BIYOS_OK ||
        BIYOS_delccontolrg(model, 0, 2) != BIYOS_OK ||
        BIYOS_delcconsp(model, 0, NULL, map) != BIYOS_OK || map[1] != 1 ||
        BIYOS_delcconrg(model, 2, 0, NULL) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_nodes(BIYOS_model *model)
{
    BIYOS_offset x = -1;
    BIYOS_offset children[2] = {-1, -1};
    const BIYOS_real weights[3] = {0.0, 1.0, 2.0};
    BIYOS_offset sum = -1;
    if (BIYOS_addvarnode(model, 1, &x) != BIYOS_OK ||
        BIYOS_addnode(model, BIYOS_OP_EXP, 1, &x, NULL, &children[0]) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    children[1] = x;
    if (BIYOS_addnode(model, BIYOS_OP_SUM, 2, children, weights, &sum) != BIYOS_OK || x != 0 ||
        children[0] != 1 || sum != 2)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_postfix(BIYOS_model *model)
{
    const BIYOS_offset off[3] = {0, 2, 4};
    const BIYOS_int code[4] = {BIYOS_OP_NODE, BIYOS_OP_SIN, BIYOS_OP_VAR, BIYOS_OP_CPOW};
    const BIYOS_offset arg[4] = {2, 1, 0, 1};
    const BIYOS_real val[1] = {3.0};
    BIYOS_offset node[2] = {-1, -1};
    const BIYOS_index idx[1] = {1};
    if (BIYOS_addexpr(model, 2, off, code, arg, val, node) != BIYOS_OK || node[0] != 3 ||
        node[1] != 5 || BIYOS_attachexprsp(model, 1, idx, node) != BIYOS_OK ||
        BIYOS_attachexprrg(model, 0, 1, node + 1) != BIYOS_OK ||
        BIYOS_clrfunsp(model, 1, idx) != BIYOS_OK ||
        BIYOS_attachexprsp(model, 1, idx, node) != BIYOS_OK ||
        BIYOS_clrfunrg(model, 0, 1) != BIYOS_OK ||
        BIYOS_attachexprrg(model, 0, 1, node + 1) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static int evaluate(const BIYOS_cbreq *req, BIYOS_real *out, void *data)
{
    out[0] = req->x[0];
    return data == NULL ? 1 : 0;
}

static BIYOS_status use_callbacks(BIYOS_model *model)
{
    const BIYOS_offset off[2] = {0, 1};
    const BIYOS_index variables[1] = {0};
    BIYOS_index function = -1;
    BIYOS_index cb = -1;
    int tag = 0;
    void *data = NULL;
    if (BIYOS_addfun(model, 1, &function) != BIYOS_OK ||
        BIYOS_addcb(model, 1, &function, &tag, &cb) != BIYOS_OK || cb != 0 ||
        BIYOS_setcbf(model, cb, evaluate) != BIYOS_OK ||
        BIYOS_setcbjac(model, cb, off, variables, evaluate) != BIYOS_OK ||
        BIYOS_setcbhess(model, cb, 1, variables, variables, evaluate) != BIYOS_OK ||
        BIYOS_setcbhessvec(model, cb, evaluate) != BIYOS_OK ||
        BIYOS_getcbdata(model, cb, &data) != BIYOS_OK || data != &tag ||
        BIYOS_setcbdata(model, cb, NULL) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static int matches(const char *found, const char *wanted)
{
    return found != NULL && strcmp(found, wanted) == 0;
}

static BIYOS_status set_names(BIYOS_model *model)
{
    const BIYOS_index idx[2] = {1, 0};
    const char *name[2] = {"y", "x"};
    if (BIYOS_setmdlname(model, "header") != BIYOS_OK ||
        BIYOS_setvarnamesp(model, 2, idx, name) != BIYOS_OK ||
        BIYOS_setfunnamesp(model, 1, idx, name) != BIYOS_OK ||
        BIYOS_setconnamesp(model, 2, idx, name) != BIYOS_OK ||
        BIYOS_setobjnamesp(model, 1, idx + 1, name + 1) != BIYOS_OK ||
        BIYOS_setindnamesp(model, 1, idx, name) != BIYOS_OK ||
        BIYOS_setsosnamesp(model, 1, idx, name) != BIYOS_OK ||
        BIYOS_setcconnamesp(model, 1, idx, name) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status get_names(const BIYOS_model *model)
{
    const BIYOS_index first[1] = {0};
    const BIYOS_index both[2] = {1, 0};
    const char *title = NULL;
    const char *variables[2] = {NULL, NULL};
    const char *function = NULL;
    const char *constraint = NULL;
    const char *objective = NULL;
    const char *indicator = NULL;
    const char *sos = NULL;
    const char *complement = NULL;
    if (BIYOS_getmdlname(model, &title) != BIYOS_OK ||
        BIYOS_getvarnamesp(model, 2, both, variables) != BIYOS_OK ||
        BIYOS_getfunnamesp(model, 1, both, &function) != BIYOS_OK ||
        BIYOS_getconnamesp(model, 1, both, &constraint) != BIYOS_OK ||
        BIYOS_getobjnamesp(model, 1, first, &objective) != BIYOS_OK ||
        BIYOS_getindnamesp(model, 1, both, &indicator) != BIYOS_OK ||
        BIYOS_getsosnamesp(model, 1, both, &sos) != BIYOS_OK ||
        BIYOS_getcconnamerg(model, 1, 1, &complement) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    if (!matches(title, "header") || !matches(variables[0], "y") || !matches(variables[1], "x") ||
        !matches(function, "y") || !matches(constraint, "y") || !matches(objective, "x") ||
        !matches(indicator, "y") || !matches(sos, "y") || !matches(complement, "y"))
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status find_names(const BIYOS_model *model)
{
    const char *names[2] = {"y", "x"};
    BIYOS_index variables[2] = {-1, -1};
    BIYOS_index found[6] = {-1, -1, -1, -1, -1, -1};
    if (BIYOS_getvaridx(model, 2, names, variables) != BIYOS_OK ||
        BIYOS_getfunidx(model, 1, names, &found[0]) != BIYOS_OK ||
        BIYOS_getconidx(model, 1, names + 1, &found[1]) != BIYOS_OK ||
        BIYOS_getobjidx(model, 1, names + 1, &found[2]) != BIYOS_OK ||
        BIYOS_getindidx(model, 1, names, &found[3]) != BIYOS_OK ||
        BIYOS_getsosidx(model, 1, names, &found[4]) != BIYOS_OK ||
        BIYOS_getcconidx(model, 1, names, &found[5]) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    if (variables[0] != 1 || variables[1] != 0 || found[0] != 1 || found[1] != 0 || found[2] != 0 ||
        found[3] != 1 || found[4] != 1 || found[5] != 1)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status del_names(BIYOS_model *model)
{
    const BIYOS_index idx[1] = {1};
    const char *variable = NULL;
    const char *title = NULL;
    if (BIYOS_delvarnamesp(model, 1, idx) != BIYOS_OK ||
        BIYOS_delfunnamesp(model, 1, idx) != BIYOS_OK ||
        BIYOS_delconnamesp(model, 1, idx) != BIYOS_OK ||
        BIYOS_delobjnamesp(model, 0, NULL) != BIYOS_OK ||
        BIYOS_delindnamesp(model, 1, idx) != BIYOS_OK ||
        BIYOS_delsosnamesp(model, 1, idx) != BIYOS_OK ||
        BIYOS_delcconnamerg(model, 1, 1) != BIYOS_OK || BIYOS_delmdlname(model) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_getvarnamesp(model, 1, idx, &variable) != BIYOS_OK || !matches(variable, "") ||
        BIYOS_getmdlname(model, &title) != BIYOS_OK || !matches(title, ""))
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status name_cones(BIYOS_model *model)
{
    const int indices[1] = {1};
    const char *names[1] = {"k"};
    const char *found = NULL;
    BIYOS_index at = -1;
    if (BIYOS_setconenamesp(model, 1, indices, names) != BIYOS_OK ||
        BIYOS_getconenamerg(model, 1, 1, &found) != BIYOS_OK || !matches(found, "k") ||
        BIYOS_getconeidx(model, 1, names, &at) != BIYOS_OK || at != 1 ||
        BIYOS_setconenamerg(model, 0, 1, names) != BIYOS_OK ||
        BIYOS_delconenamerg(model, 0, 1) != BIYOS_OK ||
        BIYOS_delconenamesp(model, 1, indices) != BIYOS_OK ||
        BIYOS_getconenamesp(model, 1, indices, &found) != BIYOS_OK || !matches(found, ""))
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_names(BIYOS_model *model)
{
    if (set_names(model) != BIYOS_OK || get_names(model) != BIYOS_OK ||
        find_names(model) != BIYOS_OK || del_names(model) != BIYOS_OK)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_options(BIYOS_model *model)
{
    BIYOS_int value = -1;
    BIYOS_int source = 0;
    BIYOS_real real = 0.0;
    const char *text = NULL;
    BIYOS_mask on = 0;
    BIYOS_mask off = 0;
    if (BIYOS_setmdlboolopt(model, BIYOS_OPT_PRESOLVE, 0) != BIYOS_OK ||
        BIYOS_getmdlboolopt(model, BIYOS_OPT_PRESOLVE, &value) != BIYOS_OK || value != 0 ||
        BIYOS_setmdlintopt(model, BIYOS_OPT_SEED, 5) != BIYOS_OK ||
        BIYOS_getmdlintopt(model, BIYOS_OPT_SEED, &value) != BIYOS_OK || value != 5 ||
        BIYOS_setmdldblopt(model, BIYOS_OPT_MAXTIME, 2.0) != BIYOS_OK ||
        BIYOS_getmdldblopt(model, BIYOS_OPT_MAXTIME, &real) != BIYOS_OK ||
        BIYOS_setmdlstropt(model, BIYOS_OPT_REPRO, "header.txt") != BIYOS_OK ||
        BIYOS_getmdlstropt(model, BIYOS_OPT_REPRO, &text) != BIYOS_OK ||
        !matches(text, "header.txt") ||
        BIYOS_setmdlflgopt(model, BIYOS_OPT_SEED, 0, 0) != BIYOS_ERR_BADARG ||
        BIYOS_getmdlflgopt(model, BIYOS_OPT_SEED, &on, &off) != BIYOS_ERR_BADARG ||
        BIYOS_getmdloptsrc(model, BIYOS_OPT_SEED, &source) != BIYOS_OK ||
        source != BIYOS_OPTSRC_MDL || BIYOS_delmdlopt(model, BIYOS_OPT_SEED) != BIYOS_OK ||
        BIYOS_getmdloptsrc(model, BIYOS_OPT_SEED, &source) != BIYOS_OK ||
        source != BIYOS_OPTSRC_ENV)
    {
        return BIYOS_ERR_INTERNAL;
    }
    return BIYOS_OK;
}

static BIYOS_status use_copy(BIYOS_env *env, BIYOS_model *model)
{
    BIYOS_model *copy = NULL;
    BIYOS_index constraints = 0;
    BIYOS_status status = BIYOS_copymdl(model, env, &copy);
    if (status == BIYOS_OK && (BIYOS_getncon(copy, &constraints) != BIYOS_OK || constraints != 2))
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_freemdl(copy) != BIYOS_OK)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    return status;
}

static BIYOS_status use_readers(BIYOS_env *env, const BIYOS_model *model)
{
    BIYOS_model *copy = NULL;
    BIYOS_index count = -1;
    BIYOS_offset needed = -1;
    BIYOS_status status = BIYOS_OK;
    if (BIYOS_getnvar(model, &count) != BIYOS_OK ||
        BIYOS_getvarrg(model, 0, 0, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getvarbndsp(model, 0, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getvarkndsp(model, 0, NULL, NULL) != BIYOS_OK ||
        BIYOS_getnfun(model, &count) != BIYOS_OK || BIYOS_getcstrg(model, 0, 0, NULL) != BIYOS_OK ||
        BIYOS_getnrlintermrg(model, 0, 0, &needed) != BIYOS_OK ||
        BIYOS_getrlintermsp(model, 0, NULL, 0, &needed, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getrlintermrg(model, 0, 0, 0, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getnclintermsp(model, 0, NULL, &needed) != BIYOS_OK ||
        BIYOS_getclintermsp(model, 0, NULL, 0, &needed, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getclintermrg(model, 0, 0, 0, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getnquadtermsp(model, 0, NULL, &needed) != BIYOS_OK ||
        BIYOS_getnquadtermrg(model, 0, 0, &needed) != BIYOS_OK ||
        BIYOS_getquadtermsp(model, 0, NULL, 0, &needed, NULL, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getquadtermrg(model, 0, 0, 0, NULL, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getnexprsp(model, 0, NULL, &needed, &needed) != BIYOS_OK ||
        BIYOS_getnexprrg(model, 0, 0, &needed, &needed) != BIYOS_OK ||
        BIYOS_getexprsp(model, 0, NULL, 0, 0, NULL, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getexprrg(model, 0, 0, 0, 0, NULL, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getncon(model, &count) != BIYOS_OK ||
        BIYOS_getconrg(model, 0, 0, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getconbndsp(model, 0, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getnobj(model, &count) != BIYOS_OK ||
        BIYOS_getobjrg(model, 0, 0, NULL, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getobjsensesp(model, 0, NULL, NULL) != BIYOS_OK ||
        BIYOS_getobjpriosp(model, 0, NULL, NULL) != BIYOS_OK ||
        BIYOS_getobjcoefsp(model, 0, NULL, NULL) != BIYOS_OK ||
        BIYOS_getobjtolsp(model, 0, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getnind(model, &count) != BIYOS_OK ||
        BIYOS_getindsp(model, 0, NULL, NULL, NULL, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getindrg(model, 0, 0, NULL, NULL, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getindbndsp(model, 0, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getindbndrg(model, 0, 0, NULL, NULL) != BIYOS_OK ||
        BIYOS_getindvarsp(model, 0, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getindvarrg(model, 0, 0, NULL, NULL) != BIYOS_OK ||
        BIYOS_getnccon(model, &count) != BIYOS_OK ||
        BIYOS_getcconsp(model, 0, NULL, NULL, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getcconrg(model, 0, 0, NULL, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getcconbndsp(model, 0, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getcconbndrg(model, 0, 0, NULL, NULL) != BIYOS_OK ||
        BIYOS_getccontolsp(model, 0, NULL, NULL, NULL) != BIYOS_OK ||
        BIYOS_getccontolrg(model, 0, 0, NULL, NULL) != BIYOS_OK ||
        BIYOS_copymdl(model, env, &copy) != BIYOS_OK)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_freemdl(copy) != BIYOS_OK)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    return status;
}

static int no_error(const BIYOS_env *env)
{
    BIYOS_error error = {.size = sizeof error};
    return BIYOS_lasterror(env, &error) == BIYOS_OK && error.status == BIYOS_OK &&
           error.mdl == NULL && error.msg[0] == '\0';
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
    const int kinds[2] = {BIYOS_VAR_CONT, BIYOS_VAR_BIN};
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
    if (BIYOS_newenv(&env) != BIYOS_OK || !no_error(env))
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_setenvintopt(env, BIYOS_OPT_SEED, 3) != BIYOS_OK ||
        BIYOS_getenvintopt(env, BIYOS_OPT_SEED, &seed) != BIYOS_OK || seed != 3)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_getoptknd(env, BIYOS_OPT_SEED, &kind) != BIYOS_OK || kind != BIYOS_KND_INT)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_addenvhook(env, count, &errors, BIYOS_EVT_MESSAGE, lowest, &id) != BIYOS_OK ||
        BIYOS_setenvintopt(env, BIYOS_OPT_SEED, -1) != BIYOS_ERR_BADARG ||
        BIYOS_delenvhook(env, id) != BIYOS_OK || errors != 1)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_newmdl(env, &model) != BIYOS_OK ||
        BIYOS_addvar(model, 2, kinds, lower, upper, &first) != BIYOS_OK || first != 0 ||
        BIYOS_setvarbndsp(model, 1, indices, lower + 1, NULL) != BIYOS_OK ||
        BIYOS_setvarkndsp(model, 1, indices, kinds + 1) != BIYOS_OK ||
        BIYOS_getvarkndsp(model, 1, indices, found + 1) != BIYOS_OK || found[1] != BIYOS_VAR_BIN ||
        BIYOS_getnvar(model, &variables) != BIYOS_OK || variables != 2 ||
        BIYOS_getvarrg(model, 0, 2, found, NULL, NULL) != BIYOS_OK || found[0] != BIYOS_VAR_BIN ||
        found[1] != BIYOS_VAR_BIN)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (use_functions(model) != BIYOS_OK || use_linear(model) != BIYOS_OK ||
        use_quadratic(model) != BIYOS_OK || use_nodes(model) != BIYOS_OK ||
        use_postfix(model) != BIYOS_OK || use_constraints(model) != BIYOS_OK ||
        use_objectives(model) != BIYOS_OK || use_indicators(model) != BIYOS_OK ||
        use_sos(model) != BIYOS_OK || use_complements(model) != BIYOS_OK ||
        use_names(model) != BIYOS_OK || use_options(model) != BIYOS_OK ||
        use_callbacks(model) != BIYOS_OK || use_copy(env, model) != BIYOS_OK ||
        use_readers(env, model) != BIYOS_OK)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (use_cones(model) != BIYOS_OK || name_cones(model) != BIYOS_OK)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_freeenv(env) == BIYOS_OK || BIYOS_freemdl(model) != BIYOS_OK)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    if (BIYOS_freeenv(env) != BIYOS_OK)
    {
        status = BIYOS_ERR_INTERNAL;
    }
    return status == BIYOS_OK ? 0 : 1;
}
