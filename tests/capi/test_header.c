#include "biyos/events.h"
#include "biyos/model.h"
#include "biyos/options.h"
#include "biyos/status.h"

#include "biyos/biyos.h"

#include <stddef.h>

#if !defined(BIYOS_OK) || BIYOS_OK != 0
#error "BIYOS_OK must be zero"
#endif
#if BIYOS_INVALID_ARGUMENT > -100 || BIYOS_INVALID_ARGUMENT < -199
#error "BIYOS_INVALID_ARGUMENT must be a caller error"
#endif
#if BIYOS_NO_MEMORY > -400 || BIYOS_NO_MEMORY < -499
#error "BIYOS_NO_MEMORY must be a resource error"
#endif
#if BIYOS_INTERNAL_ERROR > -500 || BIYOS_INTERNAL_ERROR < -599
#error "BIYOS_INTERNAL_ERROR must be an internal error"
#endif
#if BIYOS_OPT_SEED != 1 || BIYOS_OPT_TIME_LIMIT != 2 || BIYOS_OPT_CHECK_LEVEL != 3 ||              \
    BIYOS_OPT_PRESOLVE != 4 || BIYOS_OPT_REPRODUCER != 5 || BIYOS_OPT_FEASIBILITY_ABS != 6 ||      \
    BIYOS_OPT_FEASIBILITY_REL != 7 || BIYOS_OPT_INTEGRALITY_TOL != 8 || BIYOS_OPT_ZERO_TOL != 9 || \
    BIYOS_OPT_CPU_TIME_LIMIT != 10
#error "a public option id changed"
#endif
#if BIYOS_KIND_BOOL != 1 || BIYOS_KIND_INT != 2 || BIYOS_KIND_DOUBLE != 3 ||                       \
    BIYOS_KIND_STRING != 4 || BIYOS_KIND_ENUM != 5
#error "a public option kind changed"
#endif
#if BIYOS_EVENT_MESSAGE != 1 || BIYOS_EVENT_CHECK != 2 || BIYOS_EVENT_PHASE != 3
#error "a public event kind changed"
#endif
#if BIYOS_LEVEL_DEBUG != 1 || BIYOS_LEVEL_INFO != 2 || BIYOS_LEVEL_WARNING != 3 ||                 \
    BIYOS_LEVEL_ERROR != 4
#error "a public level changed"
#endif

#if !defined(BIYOS_TERMINATION_NOT_SOLVED) || BIYOS_TERMINATION_NOT_SOLVED != 0 ||                 \
    BIYOS_TERMINATION_OPTIMAL != 101 || BIYOS_TERMINATION_LOCALLY_SOLVED != 102 ||                 \
    BIYOS_TERMINATION_ALMOST_LOCALLY_SOLVED != 103 || BIYOS_TERMINATION_ITERATION_LIMIT != 201 ||  \
    BIYOS_TERMINATION_TIME_LIMIT != 202 || BIYOS_TERMINATION_CPU_TIME_LIMIT != 203 ||              \
    BIYOS_TERMINATION_EVALUATION_LIMIT != 204 || BIYOS_TERMINATION_USER_STOP != 205 ||             \
    BIYOS_TERMINATION_INFEASIBLE != 301 || BIYOS_TERMINATION_LOCALLY_INFEASIBLE != 302 ||          \
    BIYOS_TERMINATION_UNBOUNDED != 303 || BIYOS_TERMINATION_SLOW_PROGRESS != 401 ||                \
    BIYOS_TERMINATION_NUMERICAL_TROUBLE != 402
#error "a public termination changed"
#endif
#if !defined(BIYOS_RESULT_NONE) || BIYOS_RESULT_NONE != 0 || BIYOS_RESULT_FEASIBLE != 1 ||         \
    BIYOS_RESULT_NEARLY_FEASIBLE != 2 || BIYOS_RESULT_INFEASIBLE != 3 ||                           \
    BIYOS_RESULT_CERTIFICATE != 4 || BIYOS_RESULT_NEARLY_CERTIFICATE != 5 ||                       \
    BIYOS_RESULT_UNKNOWN != 6
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

static void count(const biyos_event *event, void *user_data)
{
    int *errors = user_data;
    if (biyos_event_level(event) == BIYOS_LEVEL_ERROR)
    {
        ++*errors;
    }
}

int main(void)
{
    biyos_status status = BIYOS_OK;
    biyos_env *env = NULL;
    biyos_model *model = NULL;
    int major = -1;
    int minor = -1;
    int patch = -1;
    int seed = -1;
    int kind = 0;
    int lowest = BIYOS_LEVEL_DEBUG;
    int errors = 0;
    int id = 0;
    const int kinds[2] = {BIYOS_VAR_CONTINUOUS, BIYOS_VAR_BINARY};
    const double lower[2] = {-1.0, 0.0};
    const double upper[2] = {1.0, 1.0};
    const int indices[1] = {0};
    int found[2] = {0, 0};
    int variables = 0;
    biyos_version(&major, &minor, &patch);
    if (major < 0 || minor < 0 || patch < 0)
    {
        status = BIYOS_INTERNAL_ERROR;
    }
    if (biyos_env_create(&env) != BIYOS_OK || biyos_last_error_message(env)[0] != '\0')
    {
        status = BIYOS_INTERNAL_ERROR;
    }
    if (biyos_set_int_option(env, BIYOS_OPT_SEED, 3) != BIYOS_OK ||
        biyos_get_int_option(env, BIYOS_OPT_SEED, &seed) != BIYOS_OK || seed != 3)
    {
        status = BIYOS_INTERNAL_ERROR;
    }
    if (biyos_option_kind(env, BIYOS_OPT_SEED, &kind) != BIYOS_OK || kind != BIYOS_KIND_INT)
    {
        status = BIYOS_INTERNAL_ERROR;
    }
    if (biyos_subscribe(env, count, &errors, BIYOS_EVENT_MESSAGE, lowest, &id) != BIYOS_OK ||
        biyos_set_int_option(env, BIYOS_OPT_SEED, -1) != BIYOS_INVALID_ARGUMENT ||
        biyos_unsubscribe(env, id) != BIYOS_OK || errors != 1)
    {
        status = BIYOS_INTERNAL_ERROR;
    }
    if (biyos_model_create(env, &model) != BIYOS_OK ||
        biyos_add_variables(model, 2, kinds, lower, upper) != BIYOS_OK ||
        biyos_set_variable_bounds(model, 1, indices, lower + 1, NULL) != BIYOS_OK ||
        biyos_set_variable_kinds(model, 1, indices, kinds + 1) != BIYOS_OK ||
        biyos_variable_count(model, &variables) != BIYOS_OK || variables != 2 ||
        biyos_get_variables(model, 0, 2, found, NULL, NULL) != BIYOS_OK ||
        found[0] != BIYOS_VAR_BINARY || found[1] != BIYOS_VAR_BINARY)
    {
        status = BIYOS_INTERNAL_ERROR;
    }
    if (biyos_env_free(env) == BIYOS_OK || biyos_model_free(model) != BIYOS_OK)
    {
        status = BIYOS_INTERNAL_ERROR;
    }
    if (biyos_env_free(env) != BIYOS_OK)
    {
        status = BIYOS_INTERNAL_ERROR;
    }
    return status == BIYOS_OK ? 0 : 1;
}
