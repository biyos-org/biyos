#include <biyos/biyos.h>
#include <stdio.h>

int main(void)
{
    BIYOS_int major = 0;
    BIYOS_int minor = 0;
    BIYOS_int patch = 0;
    BIYOS_version(&major, &minor, &patch);
    printf("biyos %d.%d.%d\n", major, minor, patch);

    BIYOS_env *env = NULL;
    if (BIYOS_newenv(&env) != BIYOS_OK)
    {
        return 1;
    }
    return BIYOS_freeenv(env) == BIYOS_OK ? 0 : 1;
}
