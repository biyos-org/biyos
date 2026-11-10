#include "biyos/biyos.h"

#include <stddef.h>

#if BIYOS_OK != 0
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

int main(void)
{
    biyos_status status = BIYOS_OK;
    biyos_env *env = NULL;
    int major = -1;
    int minor = -1;
    int patch = -1;
    biyos_version(&major, &minor, &patch);
    if (major < 0 || minor < 0 || patch < 0)
    {
        status = BIYOS_INTERNAL_ERROR;
    }
    if (biyos_env_create(&env) != BIYOS_OK || biyos_last_error_message(env)[0] != '\0')
    {
        status = BIYOS_INTERNAL_ERROR;
    }
    biyos_env_free(env);
    return status == BIYOS_OK ? 0 : 1;
}
